#pragma once

#include "ar61850/dms/ber.hpp"
#include "ar61850/dms/service.hpp"
#include "ar61850/dms/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ar61850::dms {

enum class ObjectClass : std::uint8_t { LogicalDevice = 0, FileSystem = 1 };
enum class AcsiClass : std::uint8_t {
    DataObject = 0, DataSet = 1, Brcb = 2, Urcb = 3, Lcb = 4, Log = 5,
    Sgcb = 6, Gocb = 7, Gscb = 8, Msvcb = 9, Usvcb = 10
};
enum class ServiceStatus : std::uint8_t {
    NoError = 0, InstanceNotAvailable = 1, InstanceInUse = 2, AccessViolation = 3,
    AccessNotAllowedInCurrentState = 4, ParameterValueInappropriate = 5,
    ParameterValueInconsistent = 6, ClassNotSupported = 7,
    InstanceLockedByOtherClient = 8, ControlMustBeSelected = 9, TypeConflict = 10,
    FailedDueToCommunicationsConstraint = 11, FailedDueToServerConstraint = 12
};

struct AssociateRequest {
    std::optional<std::string> called_ap;
    std::uint32_t max_message_size{65000};
};

struct AssociateResponse {
    std::uint32_t max_message_size{65000};
    std::string associate_id;
    std::optional<std::uint16_t> max_outstanding_calls;
    std::optional<ServiceStatus> service_error;
};

struct GetServerDirectoryRequest {
    ObjectClass object_class{ObjectClass::LogicalDevice};
    std::optional<std::string> continue_after;
};
struct GetServerDirectoryResponse {
    std::vector<std::string> logical_devices;
    std::optional<bool> more_follows;
};

struct GetLogicalDeviceDirectoryRequest {
    std::string logical_device;
    std::optional<std::string> continue_after;
};
struct GetLogicalDeviceDirectoryResponse {
    std::vector<std::string> logical_nodes;
    std::optional<bool> more_follows;
};

struct GetLogicalNodeDirectoryRequest {
    std::string logical_node_reference;
    AcsiClass acsi_class{AcsiClass::DataObject};
    std::optional<std::string> continue_after;
};
struct GetLogicalNodeDirectoryResponse {
    std::vector<std::string> instance_names;
    std::optional<bool> more_follows;
};

struct GetDataDirectoryRequest {
    std::string data_reference;
    std::optional<std::string> continue_after;
};
struct GetDataDirectoryResponse {
    std::vector<std::string> sub_data_objects;
    std::vector<std::string> data_attributes;
    std::optional<bool> more_follows;
};

struct DataAttributeDefinition {
    std::string reference;
    FunctionalConstraint fc{FunctionalConstraint::ST};
    DataType type{DataType::Unknown};
    std::optional<std::int32_t> size;
    std::vector<DataAttributeDefinition> components;
};

struct DataObjectDefinition {
    std::string name;
    std::optional<std::string> cdc;
    std::optional<std::int32_t> count;
    std::vector<DataObjectDefinition> sub_data_definitions;
    std::vector<DataAttributeDefinition> data_attributes;
};

struct GetDataDefinitionRequest {
    std::string data_reference;
    std::optional<std::string> continue_after;
};

struct GetDataDefinitionResponse {
    std::optional<std::string> cdc;
    std::optional<std::int32_t> count;
    std::vector<DataObjectDefinition> sub_data_definitions;
    std::vector<DataAttributeDefinition> data_attributes;
    std::optional<bool> more_follows;
};

struct FcdFcdaRef {
    std::string reference;
    FunctionalConstraint fc{FunctionalConstraint::ST};
};
struct GetDataValuesRequest {
    FcdFcdaRef ref;
    bool include_element_name{false};
};

struct DataAttributeValue {
    std::string name;
    DataType type{DataType::Unknown};
    DataScalar scalar{};
    std::vector<DataAttributeValue> children;
};

struct GetDataValuesResponse {
    std::vector<DataAttributeValue> data_attribute_values;
};

struct ServiceError {
    ServiceStatus status{ServiceStatus::FailedDueToServerConstraint};
};

using Payload = std::variant<std::monostate, AssociateRequest, AssociateResponse,
    GetServerDirectoryRequest, GetServerDirectoryResponse,
    GetLogicalDeviceDirectoryRequest, GetLogicalDeviceDirectoryResponse,
    GetLogicalNodeDirectoryRequest, GetLogicalNodeDirectoryResponse,
    GetDataDirectoryRequest, GetDataDirectoryResponse,
    GetDataDefinitionRequest, GetDataDefinitionResponse,
    GetDataValuesRequest, GetDataValuesResponse,
    ServiceError>;

struct DmsPdu {
    MessageClass message_class{MessageClass::Request};
    ServiceKind service{ServiceKind::Unknown};
    std::string associate_id;
    std::optional<std::uint32_t> invoke_id;
    Payload payload{};
};

class ProtocolCodec {
public:
    explicit ProtocolCodec(ber::Limits limits = {});
    ber::Bytes encode(const DmsPdu& pdu) const;
    DmsPdu decode(std::span<const std::uint8_t> bytes) const;

private:
    ber::Limits limits_;
};

} // namespace ar61850::dms
