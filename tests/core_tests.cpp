#include "ar61850/dms/engine.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

using namespace ar61850::dms;

int main() {
    Session session{2};
    const auto id0 = session.next_invoke_id();
    const auto id1 = session.next_invoke_id();

    assert(id0 == 0);
    assert(id1 == 1);
    assert(session.register_request(id0, ServiceKind::GetServerDirectory));
    assert(session.register_request(id1, ServiceKind::GetDataValues));
    assert(!session.register_request(2, ServiceKind::GetDataValues));
    assert(session.outstanding() == 2);
    assert(session.complete_request(id0).has_value());
    assert(session.outstanding() == 1);

    TraceBuffer trace{2};
    trace.push(TraceEvent{
        .endpoint=EndpointRole::Client,
        .direction=Direction::Tx,
        .service=ServiceKind::GetServerDirectory
    });
    trace.push(TraceEvent{
        .endpoint=EndpointRole::Server,
        .direction=Direction::Rx,
        .service=ServiceKind::GetServerDirectory
    });
    trace.push(TraceEvent{
        .endpoint=EndpointRole::Server,
        .direction=Direction::Tx,
        .service=ServiceKind::GetServerDirectory
    });

    auto snapshot = trace.snapshot();
    assert(snapshot.size() == 2);
    assert(snapshot.front().sequence == 2);
    assert(snapshot.back().sequence == 3);

    Worker worker{4};
    std::atomic<int> value{0};
    assert(worker.post([&] { value.fetch_add(1); }));

    for (int i = 0; i < 1000 && value.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    assert(value.load() == 1);
    worker.stop();

    return 0;
}
