#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ar61850::dms {

using Bytes = std::vector<std::uint8_t>;

struct ObjectReference {
    std::string value;
    friend bool operator==(const ObjectReference&, const ObjectReference&) = default;
};

enum class FunctionalConstraint : std::uint8_t {
    ST, MX, SP, SV, CF, DC, SG, SE, SR, OR, BL, EX, LG, CO, Unknown
};

enum class Validity : std::uint8_t { Good, Invalid, Reserved, Questionable };
enum class Source : std::uint8_t { Process, Substituted };

struct Quality {
    Validity validity{Validity::Good};
    Source source{Source::Process};
    bool test{false};
    bool operator_blocked{false};
    friend bool operator==(const Quality&, const Quality&) = default;
};

struct Timestamp {
    std::chrono::system_clock::time_point value{};
    bool clock_failure{false};
    bool clock_not_synchronized{false};
    std::optional<std::uint8_t> time_accuracy{};
    friend bool operator==(const Timestamp&, const Timestamp&) = default;
};

struct DataValue;
using DataArray = std::vector<DataValue>;
using DataScalar = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, float, double, std::string, Bytes>;

struct DataValue {
    std::string name;
    DataScalar scalar{};
    DataArray children{};
    bool is_structure() const noexcept { return !children.empty(); }
};

struct SignalSample {
    ObjectReference reference;
    FunctionalConstraint fc{FunctionalConstraint::Unknown};
    DataValue value;
    std::optional<Quality> quality;
    std::optional<Timestamp> timestamp;
};

} // namespace ar61850::dms
