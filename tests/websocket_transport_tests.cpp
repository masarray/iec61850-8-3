#include "ar61850/dms/engine.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
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
    server_ws.automatic_reconnect = false;

    auto server_transport = std::make_unique<WebSocketTransport>(server_ws);
    auto* server_wire = server_transport.get();

    ServerRuntime server{
        ServerCore{IedModel::make_ft20_reference_model()},
        std::move(server_transport),
        64,
        128
    };
    server.start();
    const auto port = server_wire->config().port;
    assert(port != 0);

    std::mutex mutex;
    std::condition_variable cv;
    bool client_connected = false;
    std::vector<Bytes> received;

    WebSocketTransportConfig client_ws;
    client_ws.mode = WebSocketMode::ActiveConnect;
    client_ws.host = "127.0.0.1";
    client_ws.port = port;
    client_ws.access_point = "cp1";
    client_ws.automatic_reconnect = false;

    WebSocketTransport client{client_ws};
    client.set_state_handler([&](bool connected, std::string_view) {
        {
            std::scoped_lock lock(mutex);
            client_connected = connected;
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
            lock, std::chrono::seconds(5), [&] { return client_connected; });
        assert(connected);
    }

    ProtocolCodec codec;

    DmsPdu associate;
    associate.message_class = MessageClass::Association;
    associate.service = ServiceKind::Associate;
    associate.payload = AssociateRequest{
        .called_ap = std::string("cp1"),
        .max_message_size = 65000
    };
    assert(client.send(codec.encode(associate)));

    Bytes associate_wire;
    {
        std::unique_lock lock(mutex);
        const bool ready = cv.wait_for(
            lock, std::chrono::seconds(5), [&] { return received.size() >= 1; });
        assert(ready);
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
    assert(client.send(codec.encode(directory_request)));

    Bytes directory_wire;
    {
        std::unique_lock lock(mutex);
        const bool ready = cv.wait_for(
            lock, std::chrono::seconds(5), [&] { return received.size() >= 2; });
        assert(ready);
        directory_wire = received[1];
    }

    const auto directory_response = codec.decode(directory_wire);
    assert(directory_response.service == ServiceKind::GetServerDirectory);
    const auto& directory =
        std::get<GetServerDirectoryResponse>(directory_response.payload);
    assert(directory.logical_devices.size() == 1);
    assert(directory.logical_devices[0] == "LD0");

    assert(server.transport_connected());
    assert(server.dropped_messages() == 0);

    client.stop();
    server.stop();
    return 0;
}
