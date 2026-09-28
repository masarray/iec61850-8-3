#include "ar61850/dms/engine.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace ar61850::dms;

class FakeTransport final : public IWireTransport {
public:
    void set_receive_handler(ReceiveHandler handler) override { receive_ = std::move(handler); }
    void set_state_handler(StateHandler handler) override { state_ = std::move(handler); }

    void start() override {
        started_ = true;
        if (state_) state_(true, "fake connected");
    }

    void stop() noexcept override {
        started_ = false;
        if (state_) state_(false, "fake stopped");
    }

    bool send(Bytes payload) override {
        std::scoped_lock lock(mutex_);
        sent_.push_back(std::move(payload));
        cv_.notify_all();
        return started_;
    }

    void inject(Bytes payload) {
        assert(receive_);
        receive_(std::move(payload));
    }

    Bytes wait_for_message(std::size_t index) {
        std::unique_lock lock(mutex_);
        const bool ready = cv_.wait_for(
            lock, std::chrono::seconds(2), [&] { return sent_.size() > index; });
        assert(ready);
        return sent_[index];
    }

private:
    ReceiveHandler receive_;
    StateHandler state_;
    bool started_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<Bytes> sent_;
};

int main() {
    auto transport = std::make_unique<FakeTransport>();
    auto* wire = transport.get();

    ServerRuntime runtime{
        ServerCore{IedModel::make_ft20_reference_model()},
        std::move(transport),
        32,
        64
    };

    runtime.start();
    assert(runtime.running());
    assert(runtime.transport_connected());

    ProtocolCodec codec;

    DmsPdu associate;
    associate.message_class = MessageClass::Association;
    associate.service = ServiceKind::Associate;
    associate.payload = AssociateRequest{
        .called_ap = std::string("cp1"),
        .max_message_size = 65000
    };

    wire->inject(codec.encode(associate));
    const auto associate_response_wire = wire->wait_for_message(0);
    const auto associate_response = codec.decode(associate_response_wire);
    assert(associate_response.service == ServiceKind::Associate);
    assert(associate_response.associate_id == "id_cp1");

    DmsPdu request;
    request.message_class = MessageClass::Request;
    request.service = ServiceKind::GetServerDirectory;
    request.associate_id = "id_cp1";
    request.invoke_id = 0;
    request.payload = GetServerDirectoryRequest{};

    wire->inject(codec.encode(request));
    const auto directory_wire = wire->wait_for_message(1);
    const auto directory = codec.decode(directory_wire);
    assert(directory.service == ServiceKind::GetServerDirectory);
    const auto& result = std::get<GetServerDirectoryResponse>(directory.payload);
    assert(result.logical_devices.size() == 1);
    assert(result.logical_devices[0] == "LD0");

    const auto traces = runtime.trace_snapshot();
    assert(traces.size() >= 4);
    assert(runtime.dropped_messages() == 0);

    runtime.stop();
    assert(!runtime.running());
    return 0;
}
