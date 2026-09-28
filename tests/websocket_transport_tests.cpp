#include "ar61850/dms/engine.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace ar61850::dms;

int main() {
    WebSocketTransportConfig server_ws;
    server_ws.mode = WebSocketMode::PassiveListen;
    server_ws.host = "127.0.0.1";
    server_ws.port = 0;
    server_ws.access_point = "cp1";
    server_ws.subprotocol.clear();
    server_ws.automatic_reconnect = false;

    auto server_transport = std::make_unique<WebSocketTransport>(server_ws);
    auto* server_wire = server_transport.get();

    ServerRuntime server{
        ServerCore{IedModel::make_ft20_reference_model()},
        std::move(server_transport),
        64,
        128
    };
    std::mutex server_event_mutex;
    std::string server_protocol_error;
    server.set_event_handler([&](const RuntimeEvent& event) {
        if (event.kind == RuntimeEvent::Kind::DecodeOrServiceError) {
            std::scoped_lock lock(server_event_mutex);
            server_protocol_error = event.detail;
        }
    });
    server.start();
    const auto port = server_wire->config().port;
    assert(port != 0);

    // Windows CI occasionally schedules the client before IXWebSocket's
    // accept loop has entered its run state even though listen() has bound.
    // Give the background accept thread a small deterministic startup window.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    std::mutex mutex;
    std::condition_variable cv;
    bool client_connected = false;
    std::string last_state_detail;
    std::vector<Bytes> received;

    WebSocketTransportConfig client_ws;
    client_ws.mode = WebSocketMode::ActiveConnect;
    client_ws.host = "127.0.0.1";
    client_ws.port = port;
    client_ws.access_point = "cp1";
    client_ws.subprotocol.clear();
    client_ws.automatic_reconnect = true;

    WebSocketTransport client{client_ws};
    client.set_state_handler([&](bool connected, std::string_view detail) {
        {
            std::scoped_lock lock(mutex);
            client_connected = connected;
            last_state_detail.assign(detail);
        }
        cv.notify_all();
    });
    client.set_receive_handler([&](Bytes payload) {
        {
            std::scoped_lock lock(mutex);
            received.push_back(std::move(payload));
        }
        cv.notify_all();
    });
    client.start();

    {
        std::unique_lock lock(mutex);
        const bool connected = cv.wait_for(
            lock, std::chrono::seconds(15), [&] { return client_connected; });
        if (!connected) {
            std::cerr << "WebSocket client failed to connect: "
                      << last_state_detail << std::endl;
            return 2;
        }
    }

    auto wait_received = [&](std::size_t count, const char* stage) -> bool {
        std::unique_lock lock(mutex);
        const bool ready = cv.wait_for(
            lock, std::chrono::seconds(10), [&] { return received.size() >= count; });
        if (!ready) {
            std::cerr << "WebSocket timeout at " << stage
                      << ": received=" << received.size()
                      << " client_connected=" << (client_connected ? "true" : "false")
                      << " state='" << last_state_detail << "'"
                      << " server_transport_connected="
                      << (server.transport_connected() ? "true" : "false")
                      << " traces=" << server.trace_snapshot().size();
            {
                std::scoped_lock event_lock(server_event_mutex);
                if (!server_protocol_error.empty()) {
                    std::cerr << " protocol_error='" << server_protocol_error << "'";
                }
            }
            std::cerr << std::endl;
        }
        return ready;
    };

    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!server.transport_connected() &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!server.transport_connected()) {
            std::cerr << "Passive server did not publish connected state before traffic"
                      << std::endl;
            return 3;
        }
    }

    ProtocolCodec codec;

    DmsPdu associate;
    associate.message_class = MessageClass::Association;
    associate.service = ServiceKind::Associate;
    associate.payload = AssociateRequest{
        .called_ap = std::string("cp1"),
        .max_message_size = 65000
    };
    if (!client.send(codec.encode(associate))) {
        std::cerr << "WebSocket client failed to send associate request; state='"
                  << last_state_detail << "' buffered transport not ready" << std::endl;
        return 10;
    }

    Bytes associate_wire;
    if (!wait_received(1, "associate response")) return 11;
    {
        std::scoped_lock lock(mutex);
        associate_wire = received[0];
    }

    const auto associate_response = codec.decode(associate_wire);
    assert(associate_response.service == ServiceKind::Associate);
    assert(associate_response.associate_id == "id_cp1");

    DmsPdu directory_request;
    directory_request.message_class = MessageClass::Request;
    directory_request.service = ServiceKind::GetServerDirectory;
    directory_request.associate_id = "id_cp1";
    directory_request.invoke_id = 0;
    directory_request.payload = GetServerDirectoryRequest{};
    if (!client.send(codec.encode(directory_request))) {
        std::cerr << "WebSocket client failed to send server-directory request" << std::endl;
        return 20;
    }

    Bytes directory_wire;
    if (!wait_received(2, "server-directory response")) return 12;
    {
        std::scoped_lock lock(mutex);
        directory_wire = received[1];
    }

    const auto directory_response = codec.decode(directory_wire);
    assert(directory_response.service == ServiceKind::GetServerDirectory);
    const auto& directory =
        std::get<GetServerDirectoryResponse>(directory_response.payload);
    assert(directory.logical_devices.size() == 1);
    assert(directory.logical_devices[0] == "LD0");

    DmsPdu definition_request;
    definition_request.message_class = MessageClass::Request;
    definition_request.service = ServiceKind::GetDataDefinition;
    definition_request.associate_id = "id_cp1";
    definition_request.invoke_id = 1;
    definition_request.payload = GetDataDefinitionRequest{
        .data_reference = "LD0/MMXU1.TotW"
    };
    if (!client.send(codec.encode(definition_request))) {
        std::cerr << "WebSocket client failed to send data-definition request" << std::endl;
        return 30;
    }

    Bytes definition_wire;
    if (!wait_received(3, "data-definition response")) return 13;
    {
        std::scoped_lock lock(mutex);
        definition_wire = received[2];
    }

    const auto definition_response = codec.decode(definition_wire);
    assert(definition_response.service == ServiceKind::GetDataDefinition);
    const auto& definition =
        std::get<GetDataDefinitionResponse>(definition_response.payload);
    assert(definition.cdc && *definition.cdc == "MV");
    assert(definition.data_attributes.size() == 4);

    DmsPdu value_request;
    value_request.message_class = MessageClass::Request;
    value_request.service = ServiceKind::GetDataValues;
    value_request.associate_id = "id_cp1";
    value_request.invoke_id = 2;
    value_request.payload = GetDataValuesRequest{
        .ref = FcdFcdaRef{
            .reference = "LD0/MMXU1.TotW",
            .fc = FunctionalConstraint::MX
        },
        .include_element_name = true
    };
    if (!client.send(codec.encode(value_request))) {
        std::cerr << "WebSocket client failed to send data-values request" << std::endl;
        return 40;
    }

    Bytes value_wire;
    if (!wait_received(4, "data-values response")) return 14;
    {
        std::scoped_lock lock(mutex);
        value_wire = received[3];
    }

    const auto value_response = codec.decode(value_wire);
    assert(value_response.service == ServiceKind::GetDataValues);
    const auto& live =
        std::get<GetDataValuesResponse>(value_response.payload);
    assert(live.data_attribute_values.size() == 3);
    assert(live.data_attribute_values[0].name == "mag");
    assert(live.data_attribute_values[0].children.size() == 1);
    assert(std::get<float>(
        live.data_attribute_values[0].children[0].scalar) == 10.0F);

    assert(server.transport_connected());
    assert(server.dropped_messages() == 0);

    const auto trace = server.trace_snapshot();
    assert(trace.size() >= 8);
    bool saw_associate = false;
    bool saw_get_values = false;
    for (const auto& event : trace) {
        if (event.service == ServiceKind::Associate) saw_associate = true;
        if (event.service == ServiceKind::GetDataValues) saw_get_values = true;
    }
    assert(saw_associate);
    assert(saw_get_values);

    client.stop();
    server.stop();
    return 0;
}
