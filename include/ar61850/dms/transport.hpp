#pragma once

#include "ar61850/dms/types.hpp"

#include <functional>
#include <string_view>

namespace ar61850::dms {

class IWireTransport {
public:
    using ReceiveHandler = std::function<void(Bytes)>;
    using StateHandler = std::function<void(bool connected, std::string_view detail)>;

    virtual ~IWireTransport() = default;
    virtual void set_receive_handler(ReceiveHandler handler) = 0;
    virtual void set_state_handler(StateHandler handler) = 0;
    virtual void start() = 0;
    virtual void stop() noexcept = 0;
    virtual bool send(Bytes payload) = 0;
};

} // namespace ar61850::dms
