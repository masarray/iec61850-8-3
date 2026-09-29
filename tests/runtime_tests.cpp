#include "ar61850/dms/engine.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
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
            lock, std::chrono::seconds(10), [&] { return sent_.size() > index; });
        if (!ready) {
            std::cerr << "FakeTransport timeout waiting for response index "
                      << index << ", sent=" << sent_.size() << std::endl;
            return {};
        }
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
    assert(!associate_response_wire.empty());
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
    assert(!directory_wire.empty());
    const auto directory = codec.decode(directory_wire);
    assert(directory.service == ServiceKind::GetServerDirectory);
    const auto& result = std::get<GetServerDirectoryResponse>(directory.payload);
    assert(result.logical_devices.size() == 1);
    assert(result.logical_devices[0] == "LD0");

    DmsPdu set_urcb;
    set_urcb.message_class = MessageClass::Request;
    set_urcb.service = ServiceKind::SetUrcbValues;
    set_urcb.associate_id = "id_cp1";
    set_urcb.invoke_id = 1;
    SetReportControlValuesRequest set_values;
    set_values.reference = "LD0/LLN0.rcbActualValues";
    set_values.enabled = true;
    set_values.gi = true;
    set_values.triggers = TriggerOptions{
        .data_change = true,
        .quality_change = true,
        .general_interrogation = true
    };
    set_urcb.payload = set_values;

    wire->inject(codec.encode(set_urcb));
    const auto set_response_wire = wire->wait_for_message(2);
    assert(!set_response_wire.empty());
    const auto set_response = codec.decode(set_response_wire);
    assert(set_response.service == ServiceKind::SetUrcbValues);

    const auto gi_wire = wire->wait_for_message(3);
    assert(!gi_wire.empty());
    const auto gi = codec.decode(gi_wire);
    assert(gi.message_class == MessageClass::Unconfirmed);
    assert(gi.service == ServiceKind::Report);
    const auto& gi_report = std::get<ReportPdu>(gi.payload);
    assert(gi_report.entries.size() == 11);
    assert(gi_report.entries.front().reason.general_interrogation);

    assert(runtime.set_float("LD0/MMXU1.TotW.mag.f", 88.0F));
    const auto dchg_wire = wire->wait_for_message(4);
    assert(!dchg_wire.empty());
    const auto dchg = codec.decode(dchg_wire);
    assert(dchg.message_class == MessageClass::Unconfirmed);
    assert(dchg.service == ServiceKind::Report);
    const auto& dchg_report = std::get<ReportPdu>(dchg.payload);
    assert(dchg_report.entries.size() == 1);
    assert(dchg_report.entries.front().reason.data_change);

    const auto traces = runtime.trace_snapshot();
    assert(traces.size() >= 8);
    assert(runtime.dropped_messages() == 0);

    runtime.stop();
    assert(!runtime.running());
    return 0;
}
