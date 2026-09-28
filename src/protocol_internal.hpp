#pragma once
#include "ar61850/dms/protocol.hpp"
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

} // namespace ar61850::dms::detail
