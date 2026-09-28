#include "ar61850/dms/control_plane.hpp"
#include "workbench_page.hpp"

#include <ixwebsocket/IXGetFreePort.h>
#include <ixwebsocket/IXHttp.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXNetSystem.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ar61850::dms {
namespace {

void ensure_http_network_initialized() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (!ix::initNetSystem()) {
            throw std::runtime_error("unable to initialize network subsystem");
        }
        std::atexit([] { ix::uninitNetSystem(); });
    });
}

std::string json_escape(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (const unsigned char c : input) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20U) {
                static constexpr char hex[] = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[(c >> 4) & 0x0fU]);
                out.push_back(hex[c & 0x0fU]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

std::string data_type_name(DataType type) {
    switch (type) {
    case DataType::Structure: return "structure";
    case DataType::Boolean: return "boolean";
    case DataType::Int8: return "int8";
    case DataType::Int16: return "int16";
    case DataType::Int24: return "int24";
    case DataType::Int32: return "int32";
    case DataType::Int64: return "int64";
    case DataType::UInt8: return "uint8";
    case DataType::UInt16: return "uint16";
    case DataType::UInt24: return "uint24";
    case DataType::UInt32: return "uint32";
    case DataType::Float32: return "float32";
    case DataType::OctetString: return "octetString";
    case DataType::VisibleString64: return "visString64";
    case DataType::VisibleString129: return "visString129";
    case DataType::VisibleString255: return "visString255";
    case DataType::BitString: return "bitString";
    case DataType::Quality: return "quality";
    case DataType::Timestamp: return "timestamp";
    case DataType::Enumerated: return "enumerated";
    case DataType::Check: return "check";
    case DataType::Unknown: return "unknown";
    }
    return "unknown";
}

std::string fc_name(FunctionalConstraint fc) {
    switch (fc) {
    case FunctionalConstraint::ST: return "ST";
    case FunctionalConstraint::MX: return "MX";
    case FunctionalConstraint::SP: return "SP";
    case FunctionalConstraint::SV: return "SV";
    case FunctionalConstraint::CF: return "CF";
    case FunctionalConstraint::DC: return "DC";
    case FunctionalConstraint::SG: return "SG";
    case FunctionalConstraint::SE: return "SE";
    case FunctionalConstraint::SR: return "SR";
    case FunctionalConstraint::OR: return "OR";
    case FunctionalConstraint::BL: return "BL";
    case FunctionalConstraint::EX: return "EX";
    case FunctionalConstraint::LG: return "LG";
    case FunctionalConstraint::CO: return "CO";
    case FunctionalConstraint::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string message_class_name(MessageClass message_class) {
    switch (message_class) {
    case MessageClass::Association: return "Association";
    case MessageClass::Request: return "Request";
    case MessageClass::Response: return "Response";
    case MessageClass::Unconfirmed: return "Unconfirmed";
    }
    return "Unknown";
}

std::string service_name(ServiceKind service) {
    switch (service) {
    case ServiceKind::Associate: return "Associate";
    case ServiceKind::Release: return "Release";
    case ServiceKind::Abort: return "Abort";
    case ServiceKind::GetServerDirectory: return "GetServerDirectory";
    case ServiceKind::GetLogicalDeviceDirectory: return "GetLogicalDeviceDirectory";
    case ServiceKind::GetLogicalNodeDirectory: return "GetLogicalNodeDirectory";
    case ServiceKind::GetDataDirectory: return "GetDataDirectory";
    case ServiceKind::GetDataDefinition: return "GetDataDefinition";
    case ServiceKind::GetDataValues: return "GetDataValues";
    case ServiceKind::SetDataValues: return "SetDataValues";
    case ServiceKind::GetDataSetDirectory: return "GetDataSetDirectory";
    case ServiceKind::GetDataSetValues: return "GetDataSetValues";
    case ServiceKind::CreateDataSet: return "CreateDataSet";
    case ServiceKind::DeleteDataSet: return "DeleteDataSet";
    case ServiceKind::GetBrcbValues: return "GetBRCBValues";
    case ServiceKind::SetBrcbValues: return "SetBRCBValues";
    case ServiceKind::GetUrcbValues: return "GetURCBValues";
    case ServiceKind::SetUrcbValues: return "SetURCBValues";
    case ServiceKind::Select: return "Select";
    case ServiceKind::SelectWithValue: return "SelectWithValue";
    case ServiceKind::Cancel: return "Cancel";
    case ServiceKind::Operate: return "Operate";
    case ServiceKind::TimeActivatedOperate: return "TimeActivatedOperate";
    case ServiceKind::Report: return "Report";
    case ServiceKind::CommandTermination: return "CommandTermination";
    case ServiceKind::ServiceError: return "ServiceError";
    case ServiceKind::Unknown: return "Unknown";
    }
    return "Unknown";
}

std::string scalar_json(const DataScalar& scalar) {
    if (std::holds_alternative<std::monostate>(scalar)) return "null";
    if (const auto* b = std::get_if<bool>(&scalar)) return *b ? "true" : "false";
    if (const auto* i = std::get_if<std::int64_t>(&scalar)) return std::to_string(*i);
    if (const auto* u = std::get_if<std::uint64_t>(&scalar)) return std::to_string(*u);
    if (const auto* f = std::get_if<float>(&scalar)) {
        if (!std::isfinite(*f)) return "null";
        std::ostringstream ss; ss << std::setprecision(9) << *f; return ss.str();
    }
    if (const auto* d = std::get_if<double>(&scalar)) {
        if (!std::isfinite(*d)) return "null";
        std::ostringstream ss; ss << std::setprecision(17) << *d; return ss.str();
    }
    if (const auto* s = std::get_if<std::string>(&scalar)) {
        return "\"" + json_escape(*s) + "\"";
    }
    if (const auto* bytes = std::get_if<Bytes>(&scalar)) {
        static constexpr char hex[] = "0123456789abcdef";
        std::string value;
        value.reserve(bytes->size() * 2);
        for (const auto b : *bytes) {
            value.push_back(hex[(b >> 4) & 0x0fU]);
            value.push_back(hex[b & 0x0fU]);
        }
        return "\"" + value + "\"";
    }
    if (const auto* q = std::get_if<Quality>(&scalar)) {
        return std::string("{\"validity\":") +
            std::to_string(static_cast<unsigned>(q->validity)) +
            ",\"source\":" + std::to_string(static_cast<unsigned>(q->source)) +
            ",\"test\":" + (q->test ? "true" : "false") +
            ",\"operatorBlocked\":" + (q->operator_blocked ? "true" : "false") + "}";
    }
    if (const auto* t = std::get_if<Timestamp>(&scalar)) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            t->value.time_since_epoch()).count();
        return std::string("{\"epochMs\":") + std::to_string(ms) +
            ",\"clockFailure\":" + (t->clock_failure ? "true" : "false") +
            ",\"clockNotSynchronized\":" +
            (t->clock_not_synchronized ? "true" : "false") + "}";
    }
    return "null";
}

void append_attributes_json(
    std::ostringstream& out,
    const std::vector<DataAttributeNode>& attrs,
    std::string_view prefix) {
    out << '[';
    bool first = true;
    for (const auto& attr : attrs) {
        if (!first) out << ',';
        first = false;
        const std::string ref = prefix.empty()
            ? attr.name : std::string(prefix) + "." + attr.name;
        out << "{\"name\":\"" << json_escape(attr.name)
            << "\",\"ref\":\"" << json_escape(ref)
            << "\",\"fc\":\"" << fc_name(attr.fc)
            << "\",\"type\":\"" << data_type_name(attr.type)
            << "\",\"value\":" << scalar_json(attr.value)
            << ",\"children\":";
        append_attributes_json(out, attr.children, ref);
        out << '}';
    }
    out << ']';
}

void append_objects_json(
    std::ostringstream& out,
    const std::vector<DataObjectNode>& objects,
    std::string_view ln_ref) {
    out << '[';
    bool first = true;
    for (const auto& object : objects) {
        if (!first) out << ',';
        first = false;
        const std::string object_ref = std::string(ln_ref) + "." + object.name;
        out << "{\"name\":\"" << json_escape(object.name)
            << "\",\"ref\":\"" << json_escape(object_ref)
            << "\",\"cdc\":\"" << json_escape(object.cdc)
            << "\",\"dataObjects\":";
        append_objects_json(out, object.children, object_ref);
        out << ",\"dataAttributes\":";
        append_attributes_json(out, object.attributes, object_ref);
        out << '}';
    }
    out << ']';
}

std::string model_json(const IedModel& model) {
    std::ostringstream out;
    out << "{\"ied\":\"" << json_escape(model.ied_name()) << "\",\"logicalDevices\":[";
    bool first_ld = true;
    for (const auto& ld : model.logical_devices()) {
        if (!first_ld) out << ',';
        first_ld = false;
        out << "{\"name\":\"" << json_escape(ld.name) << "\",\"logicalNodes\":[";
        bool first_ln = true;
        for (const auto& ln : ld.logical_nodes) {
            if (!first_ln) out << ',';
            first_ln = false;
            const std::string ln_ref = ld.name + "/" + ln.name;
            out << "{\"name\":\"" << json_escape(ln.name)
                << "\",\"ref\":\"" << json_escape(ln_ref)
                << "\",\"dataObjects\":";
            append_objects_json(out, ln.data_objects, ln_ref);
            out << '}';
        }
        out << "]}";
    }
    out << "]}";
    return out.str();
}

std::string traces_json(const std::vector<TraceEvent>& traces) {
    std::ostringstream out;
    out << '[';
    bool first = true;
    for (const auto& e : traces) {
        if (!first) out << ',';
        first = false;
        const auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            e.observed_at.time_since_epoch()).count();
        out << "{\"sequence\":" << e.sequence
            << ",\"epochMs\":" << epoch_ms
            << ",\"direction\":\"" << (e.direction == Direction::Rx ? "RX" : "TX")
            << "\",\"messageClass\":\"" << message_class_name(e.message_class)
            << "\",\"service\":\"" << service_name(e.service)
            << "\",\"bytes\":" << e.byte_count;
        if (e.invoke_id) out << ",\"invokeId\":" << *e.invoke_id;
        if (!e.associate_id.empty()) {
            out << ",\"associateId\":\"" << json_escape(e.associate_id) << "\"";
        }
        out << '}';
    }
    out << ']';
    return out.str();
}

std::string url_decode(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '+' ) {
            out.push_back(' ');
        } else if (input[i] == '%' && i + 2 < input.size()) {
            unsigned value = 0;
            const auto hex = input.substr(i + 1, 2);
            const auto result = std::from_chars(hex.data(), hex.data() + hex.size(), value, 16);
            if (result.ec == std::errc{}) {
                out.push_back(static_cast<char>(value));
                i += 2;
            } else {
                out.push_back(input[i]);
            }
        } else {
            out.push_back(input[i]);
        }
    }
    return out;
}

std::optional<std::string> query_value(std::string_view uri, std::string_view key) {
    const auto q = uri.find('?');
    if (q == std::string_view::npos) return std::nullopt;
    auto query = uri.substr(q + 1);
    while (!query.empty()) {
        const auto amp = query.find('&');
        const auto part = query.substr(0, amp);
        const auto eq = part.find('=');
        if (eq != std::string_view::npos && part.substr(0, eq) == key) {
            return url_decode(part.substr(eq + 1));
        }
        if (amp == std::string_view::npos) break;
        query.remove_prefix(amp + 1);
    }
    return std::nullopt;
}

std::string path_only(std::string_view uri) {
    const auto q = uri.find('?');
    return std::string(uri.substr(0, q));
}

ix::HttpResponsePtr response(int status, std::string body, std::string content_type = "application/json; charset=utf-8") {
    ix::WebSocketHttpHeaders headers;
    headers["Content-Type"] = std::move(content_type);
    headers["Cache-Control"] = "no-store";
    headers["X-Content-Type-Options"] = "nosniff";
    headers["X-Frame-Options"] = "DENY";
    headers["Content-Security-Policy"] =
        "default-src 'self'; style-src 'self' 'unsafe-inline'; "
        "script-src 'self' 'unsafe-inline'; connect-src 'self'; "
        "img-src 'self' data:; frame-ancestors 'none'";
    return std::make_shared<ix::HttpResponse>(
        status,
        status >= 200 && status < 300 ? "OK" : "Error",
        ix::HttpErrorCode::Ok,
        headers,
        std::move(body));
}

} // namespace

class ControlPlane::Impl {
public:
    Impl(ServerRuntime& runtime, ControlPlaneConfig config)
        : runtime_(runtime), config_(std::move(config)) {
        if (config_.host.empty()) throw std::invalid_argument("control-plane host cannot be empty");
    }

    ~Impl() { stop(); }

    void start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;

        try {
            ensure_http_network_initialized();
            if (config_.port == 0) {
                const auto port = ix::getFreePort();
                if (port <= 0 || port > 65535) {
                    throw std::runtime_error("unable to allocate control-plane port");
                }
                config_.port = static_cast<std::uint16_t>(port);
            }

            server_ = std::make_unique<ix::HttpServer>(
                static_cast<int>(config_.port), config_.host);
            server_->setOnConnectionCallback(
                [this](ix::HttpRequestPtr request,
                       std::shared_ptr<ix::ConnectionState>) -> ix::HttpResponsePtr {
                    return handle(std::move(request));
                });

            const auto result = server_->listen();
            if (!result.first) throw std::runtime_error(result.second);
            server_->start();
        } catch (...) {
            running_.store(false, std::memory_order_release);
            server_.reset();
            throw;
        }
    }

    void stop() noexcept {
        if (!running_.exchange(false, std::memory_order_acq_rel)) return;
        try {
            if (server_) server_->stop();
        } catch (...) {
        }
        server_.reset();
    }

    bool running() const noexcept { return running_.load(std::memory_order_acquire); }
    const ControlPlaneConfig& config() const noexcept { return config_; }

    std::string base_uri() const {
        return "http://" + config_.host + ":" + std::to_string(config_.port);
    }

private:
    ix::HttpResponsePtr handle(ix::HttpRequestPtr request) {
        if (!request) return response(400, "{\"error\":\"invalid request\"}");
        const auto path = path_only(request->uri);

        if (request->method == "GET" && path == "/api/health") {
            std::ostringstream out;
            out << "{\"ok\":true"
                << ",\"runtimeRunning\":" << (runtime_.running() ? "true" : "false")
                << ",\"transportActive\":" << (runtime_.transport_active() ? "true" : "false")
                << ",\"transportConnected\":" << (runtime_.transport_connected() ? "true" : "false")
                << ",\"droppedMessages\":" << runtime_.dropped_messages()
                << ",\"traceCount\":" << runtime_.trace_snapshot().size()
                << '}';
            return response(200, out.str());
        }

        if (request->method == "GET" && path == "/api/model") {
            const auto model = runtime_.model_snapshot();
            if (!model) return response(503, "{\"error\":\"model unavailable\"}");
            return response(200, model_json(*model));
        }

        if (request->method == "GET" && path == "/api/traces") {
            const auto after_text = query_value(request->uri, "after");
            const auto limit_text = query_value(request->uri, "limit");

            std::uint64_t after = 0;
            std::size_t limit = 1000;
            if (after_text) {
                const auto parsed = std::from_chars(
                    after_text->data(), after_text->data() + after_text->size(), after);
                if (parsed.ec != std::errc{} ||
                    parsed.ptr != after_text->data() + after_text->size()) {
                    return response(400, "{\"error\":\"invalid after sequence\"}");
                }
            }
            if (limit_text) {
                std::uint64_t parsed_limit = 0;
                const auto parsed = std::from_chars(
                    limit_text->data(), limit_text->data() + limit_text->size(), parsed_limit);
                if (parsed.ec != std::errc{} ||
                    parsed.ptr != limit_text->data() + limit_text->size() ||
                    parsed_limit == 0) {
                    return response(400, "{\"error\":\"invalid trace limit\"}");
                }
                limit = static_cast<std::size_t>(std::min<std::uint64_t>(parsed_limit, 2000));
            }

            auto traces = runtime_.trace_snapshot();
            traces.erase(
                std::remove_if(
                    traces.begin(), traces.end(),
                    [after](const TraceEvent& event) { return event.sequence <= after; }),
                traces.end());
            if (traces.size() > limit) {
                traces.erase(
                    traces.begin(),
                    traces.begin() + static_cast<std::ptrdiff_t>(traces.size() - limit));
            }
            return response(200, traces_json(traces));
        }

        if (request->method == "POST" && path == "/api/traces/clear") {
            runtime_.clear_trace();
            return response(200, "{\"ok\":true}");
        }

        if (request->method == "POST" && path == "/api/runtime/transport/start") {
            try {
                if (!runtime_.start_transport()) {
                    return response(409, "{\"error\":\"runtime is not running\"}");
                }
                return response(200, "{\"ok\":true,\"transportActive\":true}");
            } catch (const std::exception& ex) {
                return response(
                    500,
                    "{\"error\":\"" + json_escape(ex.what()) + "\"}");
            }
        }

        if (request->method == "POST" && path == "/api/runtime/transport/stop") {
            if (!runtime_.stop_transport()) {
                return response(500, "{\"error\":\"transport stop failed\"}");
            }
            return response(200, "{\"ok\":true,\"transportActive\":false}");
        }

        if (request->method == "POST" && path == "/api/signals/float") {
            const auto ref = query_value(request->uri, "ref");
            const auto value_text = query_value(request->uri, "value");
            if (!ref || !value_text) {
                return response(400, "{\"error\":\"ref and value are required\"}");
            }

            float value = 0.0F;
            const auto parsed = std::from_chars(
                value_text->data(), value_text->data() + value_text->size(), value);
            if (parsed.ec != std::errc{} || parsed.ptr != value_text->data() + value_text->size()) {
                return response(400, "{\"error\":\"invalid float value\"}");
            }

            if (!runtime_.set_float(*ref, value)) {
                return response(404, "{\"error\":\"signal not found or not float32\"}");
            }

            return response(
                200,
                "{\"ok\":true,\"ref\":\"" + json_escape(*ref) +
                "\",\"value\":" + *value_text + "}");
        }

        if (request->method == "GET" && path == "/api/scenarios") {
            return response(
                200,
                "[{\"id\":\"nominal\",\"label\":\"Nominal\"},"
                "{\"id\":\"load-step\",\"label\":\"Load step\"},"
                "{\"id\":\"low-load\",\"label\":\"Low load\"}]");
        }

        if (request->method == "POST" && path.rfind("/api/scenarios/", 0) == 0) {
            const auto id = path.substr(std::string("/api/scenarios/").size());
            std::vector<std::pair<std::string, float>> updates;

            if (id == "nominal") {
                updates = {
                    {"LD0/MMXU1.TotW.mag.f", 10.0F},
                    {"LD0/MMXU1.TotVAr.mag.f", 20.0F},
                    {"LD0/DWMX1.WMaxSpt.mxVal.f", 6.0F}
                };
            } else if (id == "load-step") {
                updates = {
                    {"LD0/MMXU1.TotW.mag.f", 125.0F},
                    {"LD0/MMXU1.TotVAr.mag.f", 35.0F},
                    {"LD0/DWMX1.WMaxSpt.mxVal.f", 90.0F}
                };
            } else if (id == "low-load") {
                updates = {
                    {"LD0/MMXU1.TotW.mag.f", 25.0F},
                    {"LD0/MMXU1.TotVAr.mag.f", 5.0F},
                    {"LD0/DWMX1.WMaxSpt.mxVal.f", 30.0F}
                };
            } else {
                return response(404, "{\"error\":\"unknown scenario\"}");
            }

            if (!runtime_.set_float_batch(std::move(updates))) {
                return response(409, "{\"error\":\"scenario could not be applied\"}");
            }

            return response(
                200,
                "{\"ok\":true,\"scenario\":\"" + json_escape(id) + "\"}");
        }

        if (request->method == "GET" && path == "/") {
            return response(
                200,
                std::string(workbench_page_html()),
                "text/html; charset=utf-8");
        }

        return response(404, "{\"error\":\"not found\"}");
    }

    ServerRuntime& runtime_;
    ControlPlaneConfig config_;
    std::unique_ptr<ix::HttpServer> server_;
    std::atomic<bool> running_{false};
};

ControlPlane::ControlPlane(ServerRuntime& runtime, ControlPlaneConfig config)
    : impl_(std::make_unique<Impl>(runtime, std::move(config))) {}

ControlPlane::~ControlPlane() = default;

void ControlPlane::start() { impl_->start(); }
void ControlPlane::stop() noexcept { impl_->stop(); }
bool ControlPlane::running() const noexcept { return impl_->running(); }
const ControlPlaneConfig& ControlPlane::config() const noexcept { return impl_->config(); }
std::string ControlPlane::base_uri() const { return impl_->base_uri(); }

} // namespace ar61850::dms
