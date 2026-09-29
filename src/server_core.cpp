#include "ar61850/dms/server_core.hpp"

#include <algorithm>

namespace ar61850::dms {
namespace {

DataAttributeDefinition make_attribute_definition(const DataAttributeNode& attr) {
    DataAttributeDefinition def;
    def.reference = attr.name;
    def.fc = attr.fc;
    def.type = attr.type;
    for (const auto& child : attr.children) {
        def.components.push_back(make_attribute_definition(child));
    }
    return def;
}

DataObjectDefinition make_object_definition(const DataObjectNode& object) {
    DataObjectDefinition def;
    def.name = object.name;
    if (!object.cdc.empty()) def.cdc = object.cdc;
    for (const auto& child : object.children) {
        def.sub_data_definitions.push_back(make_object_definition(child));
    }
    for (const auto& attr : object.attributes) {
        def.data_attributes.push_back(make_attribute_definition(attr));
    }
    return def;
}

DataAttributeValue make_attribute_value(const DataAttributeNode& attr, bool include_name) {
    DataAttributeValue value;
    if (include_name) value.name = attr.name;
    value.type = attr.type;
    value.scalar = attr.value;
    for (const auto& child : attr.children) {
        value.children.push_back(make_attribute_value(child, include_name));
    }
    return value;
}

bool attribute_matches_fc(const DataAttributeNode& attr, FunctionalConstraint fc) {
    if (attr.fc == fc) return true;
    return std::any_of(attr.children.begin(), attr.children.end(),
        [fc](const auto& child) { return attribute_matches_fc(child, fc); });
}

DataAttributeValue make_filtered_attribute_value(
    const DataAttributeNode& attr,
    FunctionalConstraint fc,
    bool include_name) {
    DataAttributeValue value;
    if (include_name) value.name = attr.name;
    value.type = attr.type;
    value.scalar = attr.value;
    if (attr.type == DataType::Structure) {
        for (const auto& child : attr.children) {
            if (attribute_matches_fc(child, fc)) {
                value.children.push_back(make_filtered_attribute_value(child, fc, include_name));
            }
        }
    }
    return value;
}

DataAttributeValue make_data_object_value(
    const DataObjectNode& object,
    FunctionalConstraint fc,
    bool include_name) {
    DataAttributeValue value;
    if (include_name) value.name = object.name;
    value.type = DataType::Structure;

    for (const auto& child_object : object.children) {
        bool has_matching = false;
        for (const auto& attr : child_object.attributes) {
            if (attribute_matches_fc(attr, fc)) { has_matching = true; break; }
        }
        if (!has_matching) {
            for (const auto& nested : child_object.children) {
                for (const auto& attr : nested.attributes) {
                    if (attribute_matches_fc(attr, fc)) { has_matching = true; break; }
                }
                if (has_matching) break;
            }
        }
        if (has_matching) {
            value.children.push_back(make_data_object_value(child_object, fc, include_name));
        }
    }

    for (const auto& attr : object.attributes) {
        if (attribute_matches_fc(attr, fc)) {
            value.children.push_back(make_filtered_attribute_value(attr, fc, include_name));
        }
    }
    return value;
}

bool is_rcb_configuration_write(const SetReportControlValuesRequest& req) {
    return req.report_id.has_value() ||
        req.data_set.has_value() ||
        req.buffer_time_ms.has_value() ||
        req.integrity_period_ms.has_value() ||
        req.triggers.has_value() ||
        req.optional_fields.has_value() ||
        req.reserved_time_seconds.has_value();
}

void apply_rcb_write(
    ReportControlState& state,
    const SetReportControlValuesRequest& req) {
    const bool was_enabled = state.enabled;

    if (req.enabled) state.enabled = *req.enabled;
    if (req.report_id) state.report_id = *req.report_id;
    if (req.data_set) state.data_set = *req.data_set;
    if (req.buffer_time_ms) state.buffer_time_ms = *req.buffer_time_ms;
    if (req.integrity_period_ms) {
        state.integrity_period_ms = *req.integrity_period_ms;
    }
    if (req.triggers) state.triggers = *req.triggers;
    if (req.optional_fields) state.optional_fields = *req.optional_fields;
    if (req.reserved) state.reserved = *req.reserved;
    if (req.reserved_time_seconds) {
        state.reserved_time_seconds = *req.reserved_time_seconds;
    }
    if (req.gi) state.gi = *req.gi;

    if (!was_enabled && state.enabled) {
        state.sequence_number = 0;
    }

    // There is no buffered journal yet. Treat PurgeBuf as a command and
    // acknowledge it without leaving a sticky state behind.
    if (req.purge_buffer && *req.purge_buffer) {
        state.entry_id.assign(8, 0);
        state.purge_buffer = false;
    }
}

} // namespace

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
    const auto response = handle_pdu(request);
    if (!response) return std::nullopt;
    return codec_.encode(*response);
}

std::optional<DmsPdu> ServerCore::handle_pdu(const DmsPdu& request) {
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
        return rsp;
    }

    if (request.message_class == MessageClass::Association &&
        (request.service == ServiceKind::Release || request.service == ServiceKind::Abort)) {
        if (!request.invoke_id || !validate_association(request)) return std::nullopt;

        DmsPdu rsp;
        rsp.message_class = MessageClass::Association;
        rsp.service = request.service;
        rsp.associate_id = associate_id_;
        rsp.invoke_id = request.invoke_id;
        rsp.payload = ServiceError{ServiceStatus::NoError};

        associated_ = false;
        associate_id_.clear();
        return rsp;
    }

    if (request.message_class != MessageClass::Request || !request.invoke_id) return std::nullopt;
    if (!validate_association(request)) return error_for(request, ServiceStatus::AccessNotAllowedInCurrentState);

    DmsPdu rsp;
    rsp.message_class = MessageClass::Response;
    rsp.service = request.service;
    rsp.associate_id = associate_id_;
    rsp.invoke_id = request.invoke_id;

    if (request.service == ServiceKind::GetServerDirectory) {
        const auto& req = std::get<GetServerDirectoryRequest>(request.payload);
        if (req.object_class != ObjectClass::LogicalDevice) return error_for(request, ServiceStatus::ClassNotSupported);
        GetServerDirectoryResponse body;
        for (const auto& ld : model_.logical_devices()) body.logical_devices.push_back(ld.name);
        rsp.payload = std::move(body);
        return rsp;
    }

    if (request.service == ServiceKind::GetLogicalDeviceDirectory) {
        const auto& req = std::get<GetLogicalDeviceDirectoryRequest>(request.payload);
        const auto* ld = model_.find_logical_device(req.logical_device);
        if (!ld) return error_for(request, ServiceStatus::InstanceNotAvailable);
        GetLogicalDeviceDirectoryResponse body;
        for (const auto& ln : ld->logical_nodes) body.logical_nodes.push_back(ln.name);
        rsp.payload = std::move(body);
        return rsp;
    }

    if (request.service == ServiceKind::GetLogicalNodeDirectory) {
        const auto& req = std::get<GetLogicalNodeDirectoryRequest>(request.payload);
        const auto* ln = model_.find_logical_node(req.logical_node_reference);
        if (!ln) return error_for(request, ServiceStatus::InstanceNotAvailable);
        GetLogicalNodeDirectoryResponse body;
        if (req.acsi_class == AcsiClass::DataObject) {
            for (const auto& object : ln->data_objects) {
                body.instance_names.push_back(object.name);
            }
        } else if (req.acsi_class == AcsiClass::DataSet) {
            for (const auto& data_set : ln->data_sets) {
                body.instance_names.push_back(data_set.name);
            }
        } else if (req.acsi_class == AcsiClass::Brcb) {
            for (const auto& rcb : ln->report_controls) {
                if (rcb.buffered) {
                    const auto dot = rcb.reference.value.rfind('.');
                    body.instance_names.push_back(
                        dot == std::string::npos
                            ? rcb.reference.value
                            : rcb.reference.value.substr(dot + 1));
                }
            }
        } else if (req.acsi_class == AcsiClass::Urcb) {
            for (const auto& rcb : ln->report_controls) {
                if (!rcb.buffered) {
                    const auto dot = rcb.reference.value.rfind('.');
                    body.instance_names.push_back(
                        dot == std::string::npos
                            ? rcb.reference.value
                            : rcb.reference.value.substr(dot + 1));
                }
            }
        } else {
            return error_for(request, ServiceStatus::ClassNotSupported);
        }
        rsp.payload = std::move(body);
        return rsp;
    }

    if (request.service == ServiceKind::GetDataDirectory) {
        const auto& req = std::get<GetDataDirectoryRequest>(request.payload);
        const auto* object = model_.find_data_object(req.data_reference);
        if (!object) return error_for(request, ServiceStatus::InstanceNotAvailable);
        GetDataDirectoryResponse body;
        for (const auto& child : object->children) body.sub_data_objects.push_back(child.name);
        for (const auto& attr : object->attributes) body.data_attributes.push_back(attr.name);
        rsp.payload = std::move(body);
        return rsp;
    }

    if (request.service == ServiceKind::GetDataDefinition) {
        const auto& req = std::get<GetDataDefinitionRequest>(request.payload);
        const auto* object = model_.find_data_object(req.data_reference);
        if (!object) return error_for(request, ServiceStatus::InstanceNotAvailable);

        GetDataDefinitionResponse body;
        if (!object->cdc.empty()) body.cdc = object->cdc;
        for (const auto& child : object->children) {
            body.sub_data_definitions.push_back(make_object_definition(child));
        }
        for (const auto& attr : object->attributes) {
            body.data_attributes.push_back(make_attribute_definition(attr));
        }
        rsp.payload = std::move(body);
        return rsp;
    }

    if (request.service == ServiceKind::GetDataValues) {
        const auto& req = std::get<GetDataValuesRequest>(request.payload);
        GetDataValuesResponse body;

        if (const auto* attr = model_.find_data_attribute(req.ref.reference)) {
            if (!attribute_matches_fc(*attr, req.ref.fc)) {
                return error_for(request, ServiceStatus::ParameterValueInconsistent);
            }
            body.data_attribute_values.push_back(
                make_filtered_attribute_value(*attr, req.ref.fc, req.include_element_name));
            rsp.payload = std::move(body);
            return rsp;
        }

        const auto* object = model_.find_data_object(req.ref.reference);
        if (!object) return error_for(request, ServiceStatus::InstanceNotAvailable);

        const auto object_value = make_data_object_value(
            *object, req.ref.fc, req.include_element_name);
        if (object_value.children.empty()) {
            return error_for(request, ServiceStatus::InstanceNotAvailable);
        }
        body.data_attribute_values = object_value.children;
        rsp.payload = std::move(body);
        return rsp;
    }

    if (request.service == ServiceKind::GetDataSetDirectory) {
        const auto& req = std::get<GetDataSetDirectoryRequest>(request.payload);
        const auto* data_set = model_.find_data_set(req.data_set_reference);
        if (!data_set) {
            return error_for(request, ServiceStatus::InstanceNotAvailable);
        }

        GetDataSetDirectoryResponse body;
        body.members.reserve(data_set->members.size());
        for (const auto& member : data_set->members) {
            body.members.push_back(FcdFcdaRef{
                .reference = member.reference,
                .fc = member.fc
            });
        }
        body.more_follows = false;
        rsp.payload = std::move(body);
        return rsp;
    }

    if (request.service == ServiceKind::GetDataSetValues) {
        const auto& req = std::get<GetDataSetValuesRequest>(request.payload);
        const auto* data_set = model_.find_data_set(req.data_set_reference);
        if (!data_set) {
            return error_for(request, ServiceStatus::InstanceNotAvailable);
        }

        GetDataSetValuesResponse body;
        body.member_values.reserve(data_set->members.size());
        for (const auto& member : data_set->members) {
            if (const auto* attr = model_.find_data_attribute(member.reference)) {
                if (!attribute_matches_fc(*attr, member.fc)) {
                    return error_for(
                        request, ServiceStatus::ParameterValueInconsistent);
                }
                body.member_values.push_back(
                    make_filtered_attribute_value(*attr, member.fc, false));
                continue;
            }

            const auto* object = model_.find_data_object(member.reference);
            if (!object) {
                return error_for(request, ServiceStatus::InstanceNotAvailable);
            }
            auto value = make_data_object_value(*object, member.fc, false);
            if (value.children.empty()) {
                return error_for(request, ServiceStatus::InstanceNotAvailable);
            }
            body.member_values.push_back(std::move(value));
        }

        rsp.payload = std::move(body);
        return rsp;
    }

    if (request.service == ServiceKind::GetBrcbValues ||
        request.service == ServiceKind::GetUrcbValues) {
        const bool expect_buffered =
            request.service == ServiceKind::GetBrcbValues;
        const auto& req =
            std::get<GetReportControlValuesRequest>(request.payload);
        const auto* state = model_.find_report_control(req.reference);
        if (!state) {
            return error_for(request, ServiceStatus::InstanceNotAvailable);
        }
        if (state->buffered != expect_buffered) {
            return error_for(request, ServiceStatus::ClassNotSupported);
        }

        rsp.payload = GetReportControlValuesResponse{.state = *state};
        return rsp;
    }

    if (request.service == ServiceKind::SetBrcbValues ||
        request.service == ServiceKind::SetUrcbValues) {
        const bool expect_buffered =
            request.service == ServiceKind::SetBrcbValues;
        const auto& req =
            std::get<SetReportControlValuesRequest>(request.payload);
        auto* state = model_.find_report_control(req.reference);
        if (!state) {
            return error_for(request, ServiceStatus::InstanceNotAvailable);
        }
        if (state->buffered != expect_buffered) {
            return error_for(request, ServiceStatus::ClassNotSupported);
        }

        const bool disables_in_same_request =
            req.enabled.has_value() && !*req.enabled;
        if (state->enabled &&
            is_rcb_configuration_write(req) &&
            !disables_in_same_request) {
            return error_for(
                request, ServiceStatus::AccessNotAllowedInCurrentState);
        }

        if (req.data_set && !req.data_set->empty() &&
            !model_.find_data_set(*req.data_set)) {
            return error_for(
                request, ServiceStatus::ParameterValueInconsistent);
        }

        const bool target_enabled =
            req.enabled.value_or(state->enabled);
        const auto target_triggers =
            req.triggers.value_or(state->triggers);
        if (req.gi && *req.gi) {
            if (!target_enabled) {
                return error_for(
                    request, ServiceStatus::AccessNotAllowedInCurrentState);
            }
            if (!target_triggers.general_interrogation) {
                return error_for(
                    request, ServiceStatus::ParameterValueInconsistent);
            }
        }

        apply_rcb_write(*state, req);
        rsp.payload = SetReportControlValuesResponse{.ok = true};
        return rsp;
    }

    return error_for(request, ServiceStatus::ClassNotSupported);
}

} // namespace ar61850::dms
