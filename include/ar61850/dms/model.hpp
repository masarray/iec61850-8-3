#pragma once

#include "ar61850/dms/protocol.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ar61850::dms {

enum class DataType : std::uint8_t {
    Structure,
    Boolean,
    Int8,
    Int16,
    Int24,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt24,
    UInt32,
    Float32,
    OctetString,
    VisibleString64,
    VisibleString129,
    VisibleString255,
    BitString,
    Quality,
    Timestamp,
    Enumerated,
    Check,
    Unknown
};

struct DataAttributeNode {
    std::string name;
    FunctionalConstraint fc{FunctionalConstraint::ST};
    DataType type{DataType::Unknown};
    DataScalar value{};
    std::vector<DataAttributeNode> children;

    bool is_structure() const noexcept { return type == DataType::Structure; }
};

struct DataObjectNode {
    std::string name;
    std::string cdc;
    std::vector<DataObjectNode> children;
    std::vector<DataAttributeNode> attributes;
};

struct LogicalNodeModel {
    std::string name;
    std::vector<DataObjectNode> data_objects;
};

struct LogicalDeviceModel {
    std::string name;
    std::vector<LogicalNodeModel> logical_nodes;
};

class IedModel {
public:
    explicit IedModel(std::string ied_name = "IED1");

    const std::string& ied_name() const noexcept { return ied_name_; }
    std::vector<LogicalDeviceModel>& logical_devices() noexcept { return logical_devices_; }
    const std::vector<LogicalDeviceModel>& logical_devices() const noexcept { return logical_devices_; }

    const LogicalDeviceModel* find_logical_device(std::string_view name) const noexcept;
    const LogicalNodeModel* find_logical_node(std::string_view reference) const noexcept;
    const DataObjectNode* find_data_object(std::string_view reference) const noexcept;
    const DataAttributeNode* find_data_attribute(std::string_view reference) const noexcept;
    DataAttributeNode* find_data_attribute(std::string_view reference) noexcept;

    bool set_float(std::string_view reference, float value) noexcept;

    static IedModel make_ft20_reference_model();

private:
    std::string ied_name_;
    std::vector<LogicalDeviceModel> logical_devices_;
};

} // namespace ar61850::dms
