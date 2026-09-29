#pragma once

#include "ar61850/dms/model.hpp"
#include "ar61850/dms/protocol.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ar61850::dms {

struct ServerConfig {
    std::uint32_t max_message_size{65000};
    std::uint16_t max_outstanding_calls{10};
    std::string associate_id_prefix{"id_"};
    std::size_t buffered_report_capacity{256};
};

class ServerCore {
public:
    explicit ServerCore(IedModel model, ServerConfig config = {});

    std::optional<ber::Bytes> handle(std::span<const std::uint8_t> wire_message);
    std::optional<DmsPdu> handle_pdu(const DmsPdu& request);
    bool associated() const noexcept { return associated_; }
    const std::string& associate_id() const noexcept { return associate_id_; }
    const IedModel& model() const noexcept { return model_; }
    IedModel model_snapshot() const { return model_; }
    bool set_float(std::string_view reference, float value) noexcept;
    bool set_float_batch(
        const std::vector<std::pair<std::string, float>>& updates) noexcept;
    bool set_quality(std::string_view reference, Quality quality) noexcept;

    void poll_scheduled_reports(
        std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now());
    std::vector<DmsPdu> drain_unconfirmed();

    std::size_t buffered_report_count(
        std::string_view rcb_reference) const noexcept;

private:
    enum class ChangeKind : std::uint8_t {
        DataChange,
        QualityChange,
        DataUpdate
    };

    struct ChangeNotice {
        std::string reference;
        ChangeKind kind{ChangeKind::DataChange};
    };

    struct PendingTriggerBatch {
        std::chrono::steady_clock::time_point due{};
        std::vector<std::string> references;
        ReasonForInclusion reason;
    };

    DmsPdu error_for(const DmsPdu& request, ServiceStatus status) const;
    bool validate_association(const DmsPdu& request) const noexcept;

    void enqueue_reports_for_changes(
        const std::vector<ChangeNotice>& changes,
        std::chrono::steady_clock::time_point now);
    void enqueue_gi_report(ReportControlState& state);
    void queue_report(ReportControlState& state, ReportPdu report);
    void reset_schedule(const ReportControlState& state);
    void purge_buffer(std::string_view rcb_reference);
    bool replay_buffered_after(
        const ReportControlState& state,
        const Bytes& entry_id);
    void reset_client_rcb_state();

    ProtocolCodec codec_;
    IedModel model_;
    ServerConfig config_;
    bool associated_{false};
    std::string associate_id_;
    std::vector<DmsPdu> pending_unconfirmed_;

    std::unordered_map<std::string, PendingTriggerBatch>
        pending_trigger_batches_;
    std::unordered_map<
        std::string,
        std::chrono::steady_clock::time_point>
        integrity_deadlines_;
    std::unordered_map<std::string, std::deque<ReportPdu>>
        buffered_journal_;
    std::unordered_map<std::string, std::uint64_t>
        next_buffer_entry_id_;
};

} // namespace ar61850::dms
