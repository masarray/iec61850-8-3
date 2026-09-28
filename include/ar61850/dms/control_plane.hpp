#pragma once

#include "ar61850/dms/server_runtime.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace ar61850::dms {

struct ControlPlaneConfig {
    std::string host{"127.0.0.1"};
    std::uint16_t port{8080};
};

class ControlPlane final {
public:
    ControlPlane(ServerRuntime& runtime, ControlPlaneConfig config = {});
    ~ControlPlane();

    ControlPlane(const ControlPlane&) = delete;
    ControlPlane& operator=(const ControlPlane&) = delete;

    void start();
    void stop() noexcept;

    bool running() const noexcept;
    const ControlPlaneConfig& config() const noexcept;
    std::string base_uri() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ar61850::dms
