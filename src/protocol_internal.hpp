#pragma once
#include "ar61850/dms/protocol.hpp"
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace ar61850::dms::detail {
using namespace ber;

inline void require_tag(const Tlv& tlv, TagClass cls, bool constructed, std::uint32_t number, const char* label) {
    if (tlv.tag != Tag{cls, constructed, number}) {
        std::ostringstream oss;
        oss << "DMS BER: unexpected tag for " << label;
        throw Error(oss.str());
    }
}

inline Tlv unwrap_explicit(const Tlv& tlv, std::uint32_t number, const Limits& limits, const char* label) {
    require_tag(tlv, TagClass::Context, true, number, label);
    const auto inner = children(tlv, limits, 1);
    if (inner.size() != 1) throw Error(std::string("DMS BER: explicit field child count invalid for ") + label);
    return inner.front();
}

inline std::uint64_t decode_explicit_integer(const Tlv& tlv, std::uint32_t number, const Limits& limits, const char* label) {
    const auto inner = unwrap_explicit(tlv, number, limits, label);
    require_tag(inner, TagClass::Universal, false, 2, label);
    return decode_unsigned_integer(inner);
}

inline std::uint64_t decode_explicit_enum(const Tlv& tlv, std::uint32_t number, const Limits& limits, const char* label) {
    const auto inner = unwrap_explicit(tlv, number, limits, label);
    require_tag(inner, TagClass::Universal, false, 10, label);
    return decode_unsigned_integer(inner);
}

inline std::string decode_explicit_string(const Tlv& tlv, std::uint32_t number, const Limits& limits, const char* label) {
    const auto inner = unwrap_explicit(tlv, number, limits, label);
    if (inner.tag.tag_class != TagClass::Universal || inner.tag.constructed || (inner.tag.number != 12 && inner.tag.number != 26)) {
        throw Error(std::string("DMS BER: expected string for ") + label);
    }
    return decode_string(inner);
}

inline bool decode_explicit_bool(const Tlv& tlv, std::uint32_t number, const Limits& limits, const char* label) {
    const auto inner = unwrap_explicit(tlv, number, limits, label);
    require_tag(inner, TagClass::Universal, false, 1, label);
    return decode_boolean(inner);
}

inline Bytes explicit_integer(std::uint32_t n, std::uint64_t value) { const auto inner = integer(value); return context_explicit(n, inner); }
inline Bytes explicit_enum(std::uint32_t n, std::uint64_t value) { const auto inner = enumerated(value); return context_explicit(n, inner); }
inline Bytes explicit_string(std::uint32_t n, std::string_view value) { const auto inner = utf8(value); return context_explicit(n, inner); }
inline Bytes explicit_bool(std::uint32_t n, bool value) { const auto inner = boolean(value); return context_explicit(n, inner); }

inline Bytes wrap_sequence_as_context(std::uint32_t number, const Bytes& fields) {
    const auto seq = sequence(fields);
    return context_explicit(number, seq);
}

inline Bytes encode_string_list(const std::vector<std::string>& values) {
    Bytes body;
    for (const auto& value : values) append(body, utf8(value));
    return sequence(body);
}

inline std::vector<std::string> decode_string_list(const Tlv& explicit_field, std::uint32_t number, const Limits& limits, const char* label) {
    const auto list = unwrap_explicit(explicit_field, number, limits, label);
    require_tag(list, TagClass::Universal, true, 16, label);
    std::vector<std::string> result;
    for (const auto& item : children(list, limits, 2)) {
        if (item.tag.tag_class != TagClass::Universal || item.tag.constructed || (item.tag.number != 12 && item.tag.number != 26)) {
            throw Error(std::string("DMS BER: invalid string list element for ") + label);
        }
        result.push_back(decode_string(item));
    }
    return result;
}

inline Bytes encode_request_envelope(const DmsPdu& pdu, std::uint32_t service_tag, const Bytes& service_fields) {
    if (!pdu.invoke_id) throw Error("DMS BER: request missing invokeId");
    Bytes seq_body;
    append(seq_body, explicit_string(0, pdu.associate_id));
    append(seq_body, context_implicit_integer(1, *pdu.invoke_id));
    const auto service_seq = wrap_sequence_as_context(service_tag, service_fields);
    append(seq_body, context_explicit(2, service_seq));
    const auto seq = sequence(seq_body);
    return context_explicit(1, seq);
}

inline Bytes encode_response_envelope(const DmsPdu& pdu, std::uint32_t service_tag, const Bytes& service_fields) {
    if (!pdu.invoke_id) throw Error("DMS BER: response missing invokeId");
    Bytes seq_body;
    append(seq_body, explicit_string(0, pdu.associate_id));
    append(seq_body, context_implicit_integer(1, *pdu.invoke_id));
    const auto service_choice = wrap_sequence_as_context(service_tag, service_fields);
    append(seq_body, context_explicit(2, service_choice));
    const auto seq = sequence(seq_body);
    return context_explicit(2, seq);
}

struct EnvelopeView {
    std::string associate_id;
    std::uint32_t invoke_id{0};
    Tlv service_choice{};
};

inline EnvelopeView decode_envelope(const Tlv& outer, std::uint32_t outer_tag, const Limits& limits) {
    require_tag(outer, TagClass::Context, true, outer_tag, "TpaaPdu envelope");
    const auto seqs = children(outer, limits, 0);
    if (seqs.size() != 1) throw Error("DMS BER: envelope must contain one sequence");
    const auto& seq = seqs.front();
    require_tag(seq, TagClass::Universal, true, 16, "envelope sequence");
    const auto fields = children(seq, limits, 1);
    if (fields.size() < 3) throw Error("DMS BER: incomplete request/response envelope");
    const auto associate_id = decode_explicit_string(fields[0], 0, limits, "associateId");
    require_tag(fields[1], TagClass::Context, false, 1, "invokeId");
    const auto invoke = decode_unsigned_integer(fields[1]);
    if (invoke > std::numeric_limits<std::uint32_t>::max()) throw Error("DMS BER: invokeId exceeds uint32");
    require_tag(fields[2], TagClass::Context, true, 2, "service");
    const auto services = children(fields[2], limits, 2);
    if (services.size() != 1) throw Error("DMS BER: service choice invalid");
    return EnvelopeView{associate_id, static_cast<std::uint32_t>(invoke), services.front()};
}

inline std::vector<Tlv> service_fields(const Tlv& service, const Limits& limits, const char* label) {
    if (!service.tag.constructed) throw Error(std::string("DMS BER: expected constructed service for ") + label);
    const auto seqs = children(service, limits, 2);
    if (seqs.size() != 1) throw Error(std::string("DMS BER: service sequence count invalid for ") + label);
    require_tag(seqs[0], TagClass::Universal, true, 16, label);
    return children(seqs[0], limits, 3);
}


inline std::uint32_t data_type_tag(DataType type) {
    switch (type) {
    case DataType::Boolean: return 1;
    case DataType::Int8: return 2;
    case DataType::Int16: return 3;
    case DataType::Int24: return 4;
    case DataType::Int32: return 5;
    case DataType::Int64: return 6;
    case DataType::UInt8: return 7;
    case DataType::UInt16: return 9;
    case DataType::UInt24: return 10;
    case DataType::UInt32: return 11;
    case DataType::Float32: return 12;
    case DataType::OctetString: return 13;
    case DataType::VisibleString64: return 14;
    case DataType::VisibleString129: return 15;
    case DataType::VisibleString255: return 16;
    case DataType::Structure: return 18;
    case DataType::BitString: return 19;
    case DataType::Quality: return 23;
    case DataType::Timestamp: return 24;
    case DataType::Enumerated: return 25;
    case DataType::Check: return 26;
    default: return 0;
    }
}

inline DataType data_type_from_tag(std::uint32_t tag) {
    switch (tag) {
    case 1: return DataType::Boolean;
    case 2: return DataType::Int8;
    case 3: return DataType::Int16;
    case 4: return DataType::Int24;
    case 5: return DataType::Int32;
    case 6: return DataType::Int64;
    case 7: return DataType::UInt8;
    case 9: return DataType::UInt16;
    case 10: return DataType::UInt24;
    case 11: return DataType::UInt32;
    case 12: return DataType::Float32;
    case 13: return DataType::OctetString;
    case 14: return DataType::VisibleString64;
    case 15: return DataType::VisibleString129;
    case 16: return DataType::VisibleString255;
    case 18: return DataType::Structure;
    case 19: return DataType::BitString;
    case 23: return DataType::Quality;
    case 24: return DataType::Timestamp;
    case 25: return DataType::Enumerated;
    case 26: return DataType::Check;
    default: return DataType::Unknown;
    }
}

inline Bytes signed_integer_content(std::int64_t value) {
    Bytes out(8);
    std::uint64_t bits = static_cast<std::uint64_t>(value);
    for (int i = 7; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(bits & 0xffU);
        bits >>= 8;
    }
    while (out.size() > 1) {
        if (out[0] == 0x00U && (out[1] & 0x80U) == 0) out.erase(out.begin());
        else if (out[0] == 0xffU && (out[1] & 0x80U) != 0) out.erase(out.begin());
        else break;
    }
    return out;
}

inline std::int64_t decode_signed_content(std::span<const std::uint8_t> input) {
    if (input.empty() || input.size() > 8) throw Error("DMS BER: invalid signed integer width");
    std::uint64_t bits = (input[0] & 0x80U) ? std::numeric_limits<std::uint64_t>::max() : 0U;
    for (const auto b : input) bits = (bits << 8) | b;
    return static_cast<std::int64_t>(bits);
}

inline Bytes encode_type_spec(const DataAttributeDefinition& def) {
    const auto tag = data_type_tag(def.type);
    if (tag == 0) throw Error("DMS BER: unsupported DataType in TypeSpecification");

    if (def.type != DataType::Structure) {
        Bytes content;
        if ((def.type == DataType::OctetString || def.type == DataType::BitString) && def.size) {
            content = signed_integer_content(*def.size);
        }
        return tlv({TagClass::Context, false, tag}, content);
    }

    Bytes components;
    for (const auto& cmp : def.components) {
        Bytes cmp_fields;
        if (!cmp.reference.empty()) append(cmp_fields, tlv(
            {TagClass::Context, false, 0},
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(cmp.reference.data()), cmp.reference.size())));
        append(cmp_fields, context_explicit(1, encode_type_spec(cmp)));
        append(components, sequence(cmp_fields));
    }
    return tlv({TagClass::Context, true, 18}, components);
}

inline DataAttributeDefinition decode_type_spec(
    const Tlv& spec, const Limits& limits, std::size_t depth = 0) {
    if (spec.tag.tag_class != TagClass::Context) throw Error("DMS BER: invalid TypeSpecification class");
    DataAttributeDefinition out;
    out.type = data_type_from_tag(spec.tag.number);
    if (out.type == DataType::Unknown) throw Error("DMS BER: unsupported TypeSpecification tag");

    if (out.type == DataType::Structure) {
        if (!spec.tag.constructed) throw Error("DMS BER: structure type must be constructed");
        for (const auto& cmp_seq : children(spec, limits, depth + 1)) {
            require_tag(cmp_seq, TagClass::Universal, true, 16, "StructComponent");
            const auto fields = children(cmp_seq, limits, depth + 2);
            DataAttributeDefinition cmp;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, false, 0}) {
                    cmp.reference = decode_string(field);
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    const auto inner = children(field, limits, depth + 3);
                    if (inner.size() != 1) throw Error("DMS BER: invalid StructComponent type");
                    cmp = decode_type_spec(inner.front(), limits, depth + 4);
                    if (cmp.reference.empty()) {
                        for (const auto& f2 : fields) {
                            if (f2.tag == Tag{TagClass::Context, false, 0}) cmp.reference = decode_string(f2);
                        }
                    }
                }
            }
            out.components.push_back(std::move(cmp));
        }
    } else if ((out.type == DataType::OctetString || out.type == DataType::BitString) && !spec.content.empty()) {
        out.size = static_cast<std::int32_t>(decode_signed_content(spec.content));
    }
    return out;
}

inline Bytes encode_data_value(const DataAttributeValue& value) {
    const auto tag = data_type_tag(value.type);
    if (tag == 0) throw Error("DMS BER: unsupported DataType in Data");

    if (value.type == DataType::Structure) {
        Bytes fields;
        if (!value.name.empty()) append(fields, tlv(
            {TagClass::Context, false, 0},
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(value.name.data()), value.name.size())));
        Bytes sequence_of_data;
        for (const auto& child : value.children) append(sequence_of_data, encode_data_value(child));
        append(fields, context_explicit(1, sequence(sequence_of_data)));
        return tlv({TagClass::Context, true, 18}, fields);
    }

    if (value.type == DataType::Quality) {
        Quality q{};
        if (const auto* p = std::get_if<Quality>(&value.scalar)) q = *p;
        Bytes fields;
        if (q.validity != Validity::Good) append(fields, explicit_enum(1, static_cast<std::uint8_t>(q.validity)));
        if (q.source != Source::Process) append(fields, explicit_enum(2, static_cast<std::uint8_t>(q.source)));
        if (q.test) append(fields, explicit_bool(3, true));
        if (q.operator_blocked) append(fields, explicit_bool(4, true));
        return tlv({TagClass::Context, true, 23}, fields);
    }

    if (value.type == DataType::Timestamp) {
        Timestamp t{};
        if (const auto* p = std::get_if<Timestamp>(&value.scalar)) t = *p;
        const auto duration = t.value.time_since_epoch();
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds).count();
        const auto fraction = static_cast<std::uint32_t>(
            std::clamp<long double>(
                static_cast<long double>(nanos) * 16777216.0L / 1000000000.0L, 0.0L, 16777215.0L));

        Bytes tq;
        if (t.clock_failure) append(tq, explicit_bool(1, true));
        if (t.clock_not_synchronized) append(tq, explicit_bool(2, true));
        if (t.time_accuracy) append(tq, explicit_integer(3, *t.time_accuracy));

        Bytes fields;
        append(fields, explicit_integer(0, static_cast<std::uint64_t>(seconds.count())));
        append(fields, explicit_integer(1, fraction));
        append(fields, context_explicit(2, sequence(tq)));
        return tlv({TagClass::Context, true, 24}, fields);
    }

    Bytes content;
    switch (value.type) {
    case DataType::Boolean: {
        const bool b = std::get_if<bool>(&value.scalar) ? std::get<bool>(value.scalar) : false;
        content.push_back(b ? 0xffU : 0x00U);
        break;
    }
    case DataType::Int8:
    case DataType::Int16:
    case DataType::Int24:
    case DataType::Int32:
    case DataType::Int64:
        content = signed_integer_content(std::get_if<std::int64_t>(&value.scalar) ? std::get<std::int64_t>(value.scalar) : 0);
        break;
    case DataType::UInt8:
    case DataType::UInt16:
    case DataType::UInt24:
    case DataType::UInt32:
    case DataType::Enumerated: {
        auto encoded = integer(std::get_if<std::uint64_t>(&value.scalar) ? std::get<std::uint64_t>(value.scalar) : 0);
        const auto inner = read_one(encoded);
        content.assign(inner.content.begin(), inner.content.end());
        break;
    }
    case DataType::Float32: {
        const float f = std::get_if<float>(&value.scalar) ? std::get<float>(value.scalar) : 0.0F;
        std::ostringstream ss;
        ss.setf(std::ios::scientific);
        ss.precision(9);
        ss << f;
        const auto text = ss.str();
        content.push_back(0x03U);
        content.insert(content.end(), text.begin(), text.end());
        break;
    }
    case DataType::VisibleString64:
    case DataType::VisibleString129:
    case DataType::VisibleString255: {
        const auto* str = std::get_if<std::string>(&value.scalar);
        if (str) content.assign(str->begin(), str->end());
        break;
    }
    case DataType::OctetString:
    case DataType::BitString: {
        const auto* bytes = std::get_if<Bytes>(&value.scalar);
        if (bytes) content = *bytes;
        break;
    }
    default:
        break;
    }
    return tlv({TagClass::Context, false, tag}, content);
}

inline DataAttributeValue decode_data_value(
    const Tlv& data, const Limits& limits, std::size_t depth = 0) {
    if (data.tag.tag_class != TagClass::Context) throw Error("DMS BER: invalid Data choice");
    DataAttributeValue out;
    out.type = data_type_from_tag(data.tag.number);
    if (out.type == DataType::Unknown) throw Error("DMS BER: unsupported Data tag");

    if (out.type == DataType::Structure) {
        if (!data.tag.constructed) throw Error("DMS BER: Data structure must be constructed");
        const auto fields = children(data, limits, depth + 1);
        for (const auto& field : fields) {
            if (field.tag == Tag{TagClass::Context, false, 0}) {
                out.name = decode_string(field);
            } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                const auto wrapped = children(field, limits, depth + 2);
                if (wrapped.size() != 1) throw Error("DMS BER: invalid DataSequence.data");
                require_tag(wrapped.front(), TagClass::Universal, true, 16, "DataSequence.data");
                for (const auto& child : children(wrapped.front(), limits, depth + 3)) {
                    out.children.push_back(decode_data_value(child, limits, depth + 4));
                }
            }
        }
        return out;
    }

    if (out.type == DataType::Quality) {
        Quality q{};
        for (const auto& field : children(data, limits, depth + 1)) {
            if (field.tag == Tag{TagClass::Context, true, 1}) q.validity = static_cast<Validity>(decode_explicit_enum(field, 1, limits, "validity"));
            else if (field.tag == Tag{TagClass::Context, true, 2}) q.source = static_cast<Source>(decode_explicit_enum(field, 2, limits, "source"));
            else if (field.tag == Tag{TagClass::Context, true, 3}) q.test = decode_explicit_bool(field, 3, limits, "test");
            else if (field.tag == Tag{TagClass::Context, true, 4}) q.operator_blocked = decode_explicit_bool(field, 4, limits, "operatorBlock");
        }
        out.scalar = q;
        return out;
    }

    if (out.type == DataType::Timestamp) {
        Timestamp t{};
        for (const auto& field : children(data, limits, depth + 1)) {
            if (field.tag == Tag{TagClass::Context, true, 0}) {
                const auto sec = decode_explicit_integer(field, 0, limits, "secondSinceEpoch");
                t.value = std::chrono::system_clock::time_point{std::chrono::seconds{sec}};
            } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                const auto fraction = decode_explicit_integer(field, 1, limits, "fractionOfSecond");
                const auto nanos = static_cast<std::int64_t>(
                    static_cast<long double>(fraction) * 1000000000.0L / 16777216.0L);
                t.value += std::chrono::nanoseconds{nanos};
            } else if (field.tag == Tag{TagClass::Context, true, 2}) {
                const auto tqseq = unwrap_explicit(field, 2, limits, "timeQuality");
                require_tag(tqseq, TagClass::Universal, true, 16, "timeQuality");
                for (const auto& tq : children(tqseq, limits, depth + 2)) {
                    if (tq.tag == Tag{TagClass::Context, true, 1}) t.clock_failure = decode_explicit_bool(tq, 1, limits, "clockFailure");
                    else if (tq.tag == Tag{TagClass::Context, true, 2}) t.clock_not_synchronized = decode_explicit_bool(tq, 2, limits, "clockNotSynchronized");
                    else if (tq.tag == Tag{TagClass::Context, true, 3}) t.time_accuracy = static_cast<std::uint8_t>(decode_explicit_integer(tq, 3, limits, "timeAccuracy"));
                }
            }
        }
        out.scalar = t;
        return out;
    }

    switch (out.type) {
    case DataType::Boolean:
        if (data.content.size() != 1) throw Error("DMS BER: invalid boolean Data");
        out.scalar = data.content[0] != 0;
        break;
    case DataType::Int8:
    case DataType::Int16:
    case DataType::Int24:
    case DataType::Int32:
    case DataType::Int64:
        out.scalar = decode_signed_content(data.content);
        break;
    case DataType::UInt8:
    case DataType::UInt16:
    case DataType::UInt24:
    case DataType::UInt32:
    case DataType::Enumerated:
        out.scalar = decode_unsigned_integer(data);
        break;
    case DataType::Float32: {
        if (data.content.empty()) throw Error("DMS BER: empty REAL");
        if (data.content[0] == 0x03U) {
            const std::string text(reinterpret_cast<const char*>(data.content.data() + 1), data.content.size() - 1);
            out.scalar = std::stof(text);
        } else {
            throw Error("DMS BER: unsupported REAL encoding");
        }
        break;
    }
    case DataType::VisibleString64:
    case DataType::VisibleString129:
    case DataType::VisibleString255:
        out.scalar = decode_string(data);
        break;
    case DataType::OctetString:
    case DataType::BitString:
        out.scalar = Bytes(data.content.begin(), data.content.end());
        break;
    default:
        break;
    }
    return out;
}


} // namespace ar61850::dms::detail
