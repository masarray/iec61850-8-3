#include "ar61850/dms/engine.hpp"

#include <ixwebsocket/IXHttpClient.h>

#include <cassert>
#include <memory>
#include <string>
#include <utility>

using namespace ar61850::dms;

class ControlTestTransport final : public IWireTransport {
public:
    void set_receive_handler(ReceiveHandler handler) override { receive_ = std::move(handler); }
    void set_state_handler(StateHandler handler) override { state_ = std::move(handler); }

    void start() override {
        started_ = true;
        if (state_) state_(true, "control test transport");
    }
    void stop() noexcept override {
        started_ = false;
        if (state_) state_(false, "stopped");
    }
    bool send(Bytes) override { return started_; }

private:
    ReceiveHandler receive_;
    StateHandler state_;
    bool started_{false};
};

int main() {
    auto transport = std::make_unique<ControlTestTransport>();
    ServerRuntime runtime{
        ServerCore{IedModel::make_ft20_reference_model()},
        std::move(transport),
        32,
        64
    };
    runtime.start();

    ControlPlaneConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    ControlPlane control{runtime, config};
    control.start();

    assert(control.running());
    assert(control.config().port != 0);

    ix::HttpClient client;
    auto args = client.createRequest();
    args->connectTimeout = 5;
    args->transferTimeout = 5;
    args->compress = false;

    auto health = client.get(control.base_uri() + "/api/health", args);
    assert(health);
    assert(health->statusCode == 200);
    assert(health->body.find("\"runtimeRunning\":true") != std::string::npos);
    assert(health->body.find("\"transportActive\":true") != std::string::npos);
    assert(health->body.find("\"transportConnected\":true") != std::string::npos);

    auto page = client.get(control.base_uri() + "/", args);
    assert(page);
    assert(page->statusCode == 200);
    assert(page->body.find("AR61850 DMS Workbench") != std::string::npos);
    assert(page->body.find("Protocol Inspector") == std::string::npos ||
           page->body.find("Inspector") != std::string::npos);

    auto stop_transport = client.post(
        control.base_uri() + "/api/runtime/transport/stop",
        std::string{},
        args);
    assert(stop_transport);
    assert(stop_transport->statusCode == 200);

    health = client.get(control.base_uri() + "/api/health", args);
    assert(health);
    assert(health->body.find("\"transportActive\":false") != std::string::npos);

    auto start_transport = client.post(
        control.base_uri() + "/api/runtime/transport/start",
        std::string{},
        args);
    assert(start_transport);
    assert(start_transport->statusCode == 200);

    health = client.get(control.base_uri() + "/api/health", args);
    assert(health);
    assert(health->body.find("\"transportActive\":true") != std::string::npos);

    auto model = client.get(control.base_uri() + "/api/model", args);
    assert(model);
    assert(model->statusCode == 200);
    assert(model->body.find("\"ied\":\"IED1\"") != std::string::npos);
    assert(model->body.find("LD0/MMXU1.TotW.mag.f") != std::string::npos);

    auto mutate = client.post(
        control.base_uri() +
            "/api/signals/float?ref=LD0%2FMMXU1.TotW.mag.f&value=42.5",
        std::string{},
        args);
    assert(mutate);
    assert(mutate->statusCode == 200);
    assert(mutate->body.find("\"ok\":true") != std::string::npos);

    const auto snapshot = runtime.model_snapshot();
    assert(snapshot);
    const auto* attr = snapshot->find_data_attribute("LD0/MMXU1.TotW.mag.f");
    assert(attr);
    assert(std::get<float>(attr->value) == 42.5F);

    auto scenario = client.post(
        control.base_uri() + "/api/scenarios/load-step",
        std::string{},
        args);
    assert(scenario);
    assert(scenario->statusCode == 200);
    assert(scenario->body.find("\"scenario\":\"load-step\"") != std::string::npos);

    const auto scenario_snapshot = runtime.model_snapshot();
    assert(scenario_snapshot);
    attr = scenario_snapshot->find_data_attribute("LD0/MMXU1.TotW.mag.f");
    assert(attr);
    assert(std::get<float>(attr->value) == 125.0F);

    auto traces = client.get(control.base_uri() + "/api/traces?after=0&limit=32", args);
    assert(traces);
    assert(traces->statusCode == 200);
    assert(!traces->body.empty() && traces->body.front() == '[');

    auto clear = client.post(
        control.base_uri() + "/api/traces/clear",
        std::string{},
        args);
    assert(clear);
    assert(clear->statusCode == 200);

    control.stop();
    runtime.stop();
    return 0;
}
