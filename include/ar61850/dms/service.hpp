#pragma once

#include "ar61850/dms/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ar61850::dms {

enum class ServiceKind : std::uint16_t {
    Associate,
    Release,
    Abort,
    GetServerDirectory,
    GetLogicalDeviceDirectory,
    GetLogicalNodeDirectory,
    GetDataDirectory,
    GetDataDefinition,
    GetDataValues,
    SetDataValues,
    GetDataSetDirectory,
    GetDataSetValues,
    CreateDataSet,
    DeleteDataSet,
    GetBrcbValues,
    SetBrcbValues,
    GetUrcbValues,
    SetUrcbValues,
    Select,
    SelectWithValue,
    Cancel,
    Operate,
    TimeActivatedOperate,
    Report,
    CommandTermination,
    ServiceError,
    Unknown
};

enum class MessageClass : std::uint8_t { Association, Request, Response, Unconfirmed };

struct TriggerOptions {
    bool data_change{false};
    bool quality_change{false};
    bool data_update{false};
    bool integrity{false};
    bool general_interrogation{false};
};

struct ReportControlState {
    ObjectReference reference;
    std::string report_id;
    bool enabled{false};
    bool buffered{false};
    std::string data_set;
    std::uint32_t conf_rev{0};
    std::uint32_t buffer_time_ms{0};
    std::uint32_t integrity_period_ms{0};
    TriggerOptions triggers{};
    bool gi{false};
};

struct ReportEntry {
    ObjectReference reference;
    DataValue value;
    std::optional<Quality> quality;
    std::optional<Timestamp> timestamp;
    std::vector<std::string> reasons;
};

struct ReportEvent {
    std::string associate_id;
    std::string report_id;
    std::string data_set;
    std::uint32_t sequence_number{0};
    std::uint32_t conf_rev{0};
    bool more_segments_follow{false};
    std::vector<ReportEntry> entries;
};

struct MessageEnvelope {
    MessageClass message_class{MessageClass::Request};
    ServiceKind service{ServiceKind::Unknown};
    std::string associate_id;
    std::optional<std::uint32_t> invoke_id;
    Bytes wire_payload;
};

} // namespace ar61850::dms
