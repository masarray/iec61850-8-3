#pragma once

#include "ar61850/dms/model.hpp"
#include "ar61850/dms/protocol.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ar61850::dms {

struct ServerConfig {
    std::uint32_t max_message_size{65000};
    std::uint16_t max_outstanding_calls{10};
    std::string associate_id_prefix{"id_"};
};

class ServerCore {
public:
    explicit ServerCore(IedModel model, ServerConfig config = {});

    std::optional<ber::Bytes> handle(std::span<const std::uint8_t> wire_message);
    bool associated() const noexcept { return associated_; }
    const std::string& associate_id() const noexcept { return associate_id_; }
    const IedModel& model() const noexcept { return model_; }
    IedModel model_snapshot() const { return model_; }
    bool set_float(std::string_view reference, float value) noexcept {
        return model_.set_float(reference, value);
    }

private:
    DmsPdu error_for(const DmsPdu& request, ServiceStatus status) const;
    bool validate_association(const DmsPdu& request) const noexcept;

    ProtocolCodec codec_;
    IedModel model_;
    ServerConfig config_;
    bool associated_{false};
    std::string associate_id_;
};

} // namespace ar61850::dms
