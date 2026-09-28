#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ar61850::dms::ber {

using Bytes = std::vector<std::uint8_t>;

enum class TagClass : std::uint8_t { Universal = 0, Application = 1, Context = 2, Private = 3 };

struct Tag {
    TagClass tag_class{TagClass::Universal};
    bool constructed{false};
    std::uint32_t number{0};
    friend bool operator==(const Tag&, const Tag&) = default;
};

struct Limits {
    std::size_t max_message_size{1024 * 1024};
    std::size_t max_tlv_length{1024 * 1024};
    std::size_t max_children{16384};
    std::size_t max_depth{32};
};

struct Tlv {
    Tag tag{};
    std::span<const std::uint8_t> content{};
    std::size_t header_size{0};
    std::size_t encoded_size{0};
};

class Error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

Tlv read_one(std::span<const std::uint8_t> input, const Limits& limits = {});
std::vector<Tlv> children(const Tlv& parent, const Limits& limits = {}, std::size_t depth = 0);

Bytes encode_tag(Tag tag);
Bytes encode_length(std::size_t length);
Bytes tlv(Tag tag, std::span<const std::uint8_t> content);
Bytes sequence(std::span<const std::uint8_t> content);
Bytes context_explicit(std::uint32_t number, std::span<const std::uint8_t> inner);
Bytes context_implicit_integer(std::uint32_t number, std::uint64_t value);
Bytes integer(std::uint64_t value);
Bytes enumerated(std::uint64_t value);
Bytes boolean(bool value);
Bytes utf8(std::string_view value);

std::uint64_t decode_unsigned_integer(const Tlv& tlv);
std::string decode_string(const Tlv& tlv);
bool decode_boolean(const Tlv& tlv);

void append(Bytes& target, std::span<const std::uint8_t> bytes);

} // namespace ar61850::dms::ber
