#include "ar61850/dms/ber.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace ar61850::dms::ber {
namespace {

std::pair<Tag, std::size_t> decode_tag(std::span<const std::uint8_t> input) {
    if (input.empty()) throw Error("BER: missing tag");
    const auto first = input[0];
    Tag tag;
    tag.tag_class = static_cast<TagClass>((first >> 6) & 0x03U);
    tag.constructed = (first & 0x20U) != 0;
    const auto low = static_cast<std::uint32_t>(first & 0x1fU);
    if (low != 0x1fU) {
        tag.number = low;
        return {tag, 1};
    }

    std::uint32_t number = 0;
    std::size_t pos = 1;
    bool first_octet = true;
    for (; pos < input.size() && pos < 7; ++pos) {
        const auto b = input[pos];
        if (first_octet && (b & 0x7fU) == 0) throw Error("BER: non-minimal high-tag form");
        first_octet = false;
        if (number > (std::numeric_limits<std::uint32_t>::max() >> 7)) throw Error("BER: tag overflow");
        number = (number << 7) | static_cast<std::uint32_t>(b & 0x7fU);
        if ((b & 0x80U) == 0) {
            tag.number = number;
            return {tag, pos + 1};
        }
    }
    throw Error("BER: invalid high-tag form");
}

std::pair<std::size_t, std::size_t> decode_length(std::span<const std::uint8_t> input) {
    if (input.empty()) throw Error("BER: missing length");
    const auto first = input[0];
    if ((first & 0x80U) == 0) return {first, 1};

    const auto count = static_cast<std::size_t>(first & 0x7fU);
    if (count == 0) throw Error("BER: indefinite lengths are not accepted");
    if (count > sizeof(std::size_t) || input.size() < 1 + count) throw Error("BER: invalid long length");
    if (input[1] == 0) throw Error("BER: non-minimal long length");

    std::size_t value = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (value > (std::numeric_limits<std::size_t>::max() >> 8)) throw Error("BER: length overflow");
        value = (value << 8) | input[1 + i];
    }
    if (value < 128) throw Error("BER: non-minimal long length");
    return {value, 1 + count};
}

Bytes unsigned_bytes(std::uint64_t value) {
    std::array<std::uint8_t, 9> buf{};
    std::size_t pos = buf.size();
    do {
        buf[--pos] = static_cast<std::uint8_t>(value & 0xffU);
        value >>= 8;
    } while (value != 0);
    if ((buf[pos] & 0x80U) != 0) buf[--pos] = 0;
    return Bytes(buf.begin() + static_cast<std::ptrdiff_t>(pos), buf.end());
}

} // namespace

Tlv read_one(std::span<const std::uint8_t> input, const Limits& limits) {
    if (input.size() > limits.max_message_size) throw Error("BER: message exceeds configured limit");
    const auto [tag, tag_size] = decode_tag(input);
    if (input.size() <= tag_size) throw Error("BER: missing length after tag");
    const auto [length, length_size] = decode_length(input.subspan(tag_size));
    if (length > limits.max_tlv_length) throw Error("BER: TLV length exceeds configured limit");
    const auto header = tag_size + length_size;
    if (header > input.size() || length > input.size() - header) throw Error("BER: truncated TLV");
    return Tlv{tag, input.subspan(header, length), header, header + length};
}

std::vector<Tlv> children(const Tlv& parent, const Limits& limits, std::size_t depth) {
    if (!parent.tag.constructed) throw Error("BER: children requested from primitive TLV");
    if (depth >= limits.max_depth) throw Error("BER: nesting limit exceeded");
    std::vector<Tlv> result;
    std::size_t offset = 0;
    while (offset < parent.content.size()) {
        if (result.size() >= limits.max_children) throw Error("BER: child-count limit exceeded");
        auto child = read_one(parent.content.subspan(offset), limits);
        result.push_back(child);
        offset += child.encoded_size;
    }
    if (offset != parent.content.size()) throw Error("BER: malformed constructed content");
    return result;
}

Bytes encode_tag(Tag tag) {
    const auto class_bits = static_cast<std::uint8_t>(static_cast<std::uint8_t>(tag.tag_class) << 6);
    const auto constructed = static_cast<std::uint8_t>(tag.constructed ? 0x20U : 0x00U);
    if (tag.number < 31) return Bytes{static_cast<std::uint8_t>(class_bits | constructed | tag.number)};

    Bytes suffix;
    auto value = tag.number;
    do {
        suffix.push_back(static_cast<std::uint8_t>(value & 0x7fU));
        value >>= 7;
    } while (value != 0);
    std::reverse(suffix.begin(), suffix.end());
    for (std::size_t i = 0; i + 1 < suffix.size(); ++i) suffix[i] |= 0x80U;

    Bytes out{static_cast<std::uint8_t>(class_bits | constructed | 0x1fU)};
    append(out, suffix);
    return out;
}

Bytes encode_length(std::size_t length) {
    if (length < 128) return Bytes{static_cast<std::uint8_t>(length)};
    std::array<std::uint8_t, sizeof(std::size_t)> buf{};
    std::size_t pos = buf.size();
    auto value = length;
    while (value != 0) {
        buf[--pos] = static_cast<std::uint8_t>(value & 0xffU);
        value >>= 8;
    }
    const auto count = buf.size() - pos;
    Bytes out{static_cast<std::uint8_t>(0x80U | count)};
    out.insert(out.end(), buf.begin() + static_cast<std::ptrdiff_t>(pos), buf.end());
    return out;
}

Bytes tlv(Tag tag, std::span<const std::uint8_t> content) {
    auto out = encode_tag(tag);
    auto len = encode_length(content.size());
    append(out, len);
    append(out, content);
    return out;
}

Bytes sequence(std::span<const std::uint8_t> content) { return tlv({TagClass::Universal, true, 16}, content); }
Bytes context_explicit(std::uint32_t number, std::span<const std::uint8_t> inner) { return tlv({TagClass::Context, true, number}, inner); }
Bytes context_implicit_integer(std::uint32_t number, std::uint64_t value) {
    const auto body = unsigned_bytes(value);
    return tlv({TagClass::Context, false, number}, body);
}
Bytes integer(std::uint64_t value) { const auto body = unsigned_bytes(value); return tlv({TagClass::Universal, false, 2}, body); }
Bytes enumerated(std::uint64_t value) { const auto body = unsigned_bytes(value); return tlv({TagClass::Universal, false, 10}, body); }
Bytes boolean(bool value) { const std::uint8_t b = value ? 0xffU : 0x00U; return tlv({TagClass::Universal, false, 1}, std::span<const std::uint8_t>(&b, 1)); }
Bytes utf8(std::string_view value) {
    return tlv({TagClass::Universal, false, 12}, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(value.data()), value.size()));
}

std::uint64_t decode_unsigned_integer(const Tlv& value) {
    if (value.content.empty() || value.content.size() > 9) throw Error("BER: unsupported integer width");
    if ((value.content[0] & 0x80U) != 0) throw Error("BER: negative integer where unsigned expected");
    std::size_t start = 0;
    if (value.content.size() > 1 && value.content[0] == 0) start = 1;
    std::uint64_t out = 0;
    for (std::size_t i = start; i < value.content.size(); ++i) {
        if (out > (std::numeric_limits<std::uint64_t>::max() >> 8)) throw Error("BER: integer overflow");
        out = (out << 8) | value.content[i];
    }
    return out;
}

std::string decode_string(const Tlv& value) {
    return std::string(reinterpret_cast<const char*>(value.content.data()), value.content.size());
}

bool decode_boolean(const Tlv& value) {
    if (value.content.size() != 1) throw Error("BER: boolean must contain exactly one octet");
    return value.content[0] != 0;
}

void append(Bytes& target, std::span<const std::uint8_t> bytes) { target.insert(target.end(), bytes.begin(), bytes.end()); }

} // namespace ar61850::dms::ber
