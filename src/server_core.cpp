#include "ar61850/dms/server_core.hpp"

#include <algorithm>

namespace ar61850::dms {

ServerCore::ServerCore(IedModel model, ServerConfig config)
    : model_(std::move(model)), config_(std::move(config)) {}

bool ServerCore::validate_association(const DmsPdu& request) const noexcept {
    return associated_ && !request.associate_id.empty() && request.associate_id == associate_id_;
}

DmsPdu ServerCore::error_for(const DmsPdu& request, ServiceStatus status) const {
    DmsPdu rsp;
    rsp.message_class = MessageClass::Response;
    rsp.service = ServiceKind::ServiceError;
    rsp.associate_id = request.associate_id;
    rsp.invoke_id = request.invoke_id;
    rsp.payload = ServiceError{status};
    return rsp;
}

std::optional<ber::Bytes> ServerCore::handle(std::span<const std::uint8_t> wire_message) {
    const auto request = codec_.decode(wire_message);

    if (request.message_class == MessageClass::Association && request.service == ServiceKind::Associate) {
        const auto& associate = std::get<AssociateRequest>(request.payload);
        const auto called_ap = associate.called_ap.value_or("cp1");
        associate_id_ = config_.associate_id_prefix + called_ap;
        associated_ = true;

        DmsPdu rsp;
        rsp.message_class = MessageClass::Association;
        rsp.service = ServiceKind::Associate;
        rsp.associate_id = associate_id_;
        rsp.payload = AssociateResponse{
            .max_message_size = std::min(config_.max_message_size, associate.max_message_size),
            .associate_id = associate_id_,
            .max_outstanding_calls = config_.max_outstanding_calls,
            .service_error = std::nullopt
        };
        return codec_.encode(rsp);
    }

    if (request.message_class != MessageClass::Request || !request.invoke_id) return std::nullopt;
    if (!validate_association(request)) return codec_.encode(error_for(request, ServiceStatus::AccessNotAllowedInCurrentState));

    DmsPdu rsp;
    rsp.message_class = MessageClass::Response;
    rsp.service = request.service;
    rsp.associate_id = associate_id_;
    rsp.invoke_id = request.invoke_id;

    if (request.service == ServiceKind::GetServerDirectory) {
        const auto& req = std::get<GetServerDirectoryRequest>(request.payload);
        if (req.object_class != ObjectClass::LogicalDevice) return codec_.encode(error_for(request, ServiceStatus::ClassNotSupported));
        GetServerDirectoryResponse body;
        for (const auto& ld : model_.logical_devices()) body.logical_devices.push_back(ld.name);
        rsp.payload = std::move(body);
        return codec_.encode(rsp);
    }

    if (request.service == ServiceKind::GetLogicalDeviceDirectory) {
        const auto& req = std::get<GetLogicalDeviceDirectoryRequest>(request.payload);
        const auto* ld = model_.find_logical_device(req.logical_device);
        if (!ld) return codec_.encode(error_for(request, ServiceStatus::InstanceNotAvailable));
        GetLogicalDeviceDirectoryResponse body;
        for (const auto& ln : ld->logical_nodes) body.logical_nodes.push_back(ln.name);
        rsp.payload = std::move(body);
        return codec_.encode(rsp);
    }

    if (request.service == ServiceKind::GetLogicalNodeDirectory) {
        const auto& req = std::get<GetLogicalNodeDirectoryRequest>(request.payload);
        const auto* ln = model_.find_logical_node(req.logical_node_reference);
        if (!ln) return codec_.encode(error_for(request, ServiceStatus::InstanceNotAvailable));
        if (req.acsi_class != AcsiClass::DataObject) return codec_.encode(error_for(request, ServiceStatus::ClassNotSupported));
        GetLogicalNodeDirectoryResponse body;
        for (const auto& object : ln->data_objects) body.instance_names.push_back(object.name);
        rsp.payload = std::move(body);
        return codec_.encode(rsp);
    }

    if (request.service == ServiceKind::GetDataDirectory) {
        const auto& req = std::get<GetDataDirectoryRequest>(request.payload);
        const auto* object = model_.find_data_object(req.data_reference);
        if (!object) return codec_.encode(error_for(request, ServiceStatus::InstanceNotAvailable));
        GetDataDirectoryResponse body;
        for (const auto& child : object->children) body.sub_data_objects.push_back(child.name);
        for (const auto& attr : object->attributes) body.data_attributes.push_back(attr.name);
        rsp.payload = std::move(body);
        return codec_.encode(rsp);
    }

    return codec_.encode(error_for(request, ServiceStatus::ClassNotSupported));
}

} // namespace ar61850::dms
