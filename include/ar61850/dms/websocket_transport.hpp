#pragma once

#include "ar61850/dms/transport.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace ar61850::dms {

enum class WebSocketMode : std::uint8_t {
    PassiveListen,
    ActiveConnect
};

struct WebSocketTransportConfig {
    WebSocketMode mode{WebSocketMode::PassiveListen};
    std::string host{"127.0.0.1"};
    std::uint16_t port{8765};
    std::string access_point{"cp1"};
    std::string subprotocol{"iec61850-tpaa-ber-v1"};
    bool automatic_reconnect{true};
    std::size_t max_message_size{1024 * 1024};
    std::uint32_t initial_send_settle_ms{200};
};

class WebSocketTransport final : public IWireTransport {
public:
    explicit WebSocketTransport(WebSocketTransportConfig config = {});
    ~WebSocketTransport() override;

    WebSocketTransport(const WebSocketTransport&) = delete;
    WebSocketTransport& operator=(const WebSocketTransport&) = delete;

    void set_receive_handler(ReceiveHandler handler) override;
    void set_state_handler(StateHandler handler) override;
    void start() override;
    void stop() noexcept override;
    bool send(Bytes payload) override;

    const WebSocketTransportConfig& config() const noexcept;
    std::string endpoint_uri() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ar61850::dms
