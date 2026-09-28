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

struct ReportOptionalFields {
    bool sequence_number{false};
    bool timestamp{true};
    bool data_set{true};
    bool buffer_overflow{false};
    bool config_revision{false};
    bool entry_id{false};
    bool data_reference{false};
    bool reason_code{true};
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
    ReportOptionalFields optional_fields{};
    std::uint16_t sequence_number{0};
    bool gi{false};
    bool purge_buffer{false};
    bool reserved{false};
    std::int16_t reserved_time_seconds{0};
    Bytes entry_id{};
    std::optional<Timestamp> time_of_entry;
    Bytes owner{};
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
