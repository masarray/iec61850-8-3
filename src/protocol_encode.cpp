#include "protocol_internal.hpp"

namespace ar61850::dms {
using namespace detail;

ProtocolCodec::ProtocolCodec(ber::Limits limits) : limits_(limits) {}

ber::Bytes ProtocolCodec::encode(const DmsPdu& pdu) const {
    using namespace ber;

    if (pdu.message_class == MessageClass::Association && pdu.service == ServiceKind::Associate && std::holds_alternative<AssociateRequest>(pdu.payload)) {
        const auto& req = std::get<AssociateRequest>(pdu.payload);
        Bytes fields;
        if (req.called_ap) append(fields, explicit_string(0, *req.called_ap));
        append(fields, explicit_integer(1, req.max_message_size));
        const auto seq = sequence(fields);
        const auto associate_request = context_explicit(0, seq);
        const auto service = context_explicit(1, associate_request);
        return context_explicit(0, service);
    }

    if (pdu.message_class == MessageClass::Association && pdu.service == ServiceKind::Associate && std::holds_alternative<AssociateResponse>(pdu.payload)) {
        const auto& rsp = std::get<AssociateResponse>(pdu.payload);
        Bytes fields;
        append(fields, explicit_integer(0, rsp.max_message_size));
        append(fields, explicit_string(1, rsp.associate_id));
        if (rsp.max_outstanding_calls) append(fields, explicit_integer(2, *rsp.max_outstanding_calls));
        if (rsp.service_error) append(fields, explicit_enum(3, static_cast<std::uint8_t>(*rsp.service_error)));
        const auto seq = sequence(fields);
        const auto associate_response = context_explicit(1, seq);
        const auto service = context_explicit(1, associate_response);
        return context_explicit(0, service);
    }

    if (pdu.message_class == MessageClass::Request) {
        if (pdu.service == ServiceKind::GetServerDirectory) {
            const auto& req = std::get<GetServerDirectoryRequest>(pdu.payload);
            Bytes fields;
            append(fields, explicit_enum(0, static_cast<std::uint8_t>(req.object_class)));
            if (req.continue_after) append(fields, explicit_string(1, *req.continue_after));
            return encode_request_envelope(pdu, 29, fields);
        }
        if (pdu.service == ServiceKind::GetLogicalDeviceDirectory) {
            const auto& req = std::get<GetLogicalDeviceDirectoryRequest>(pdu.payload);
            Bytes fields;
            append(fields, explicit_string(0, req.logical_device));
            if (req.continue_after) append(fields, explicit_string(1, *req.continue_after));
            return encode_request_envelope(pdu, 1, fields);
        }
        if (pdu.service == ServiceKind::GetLogicalNodeDirectory) {
            const auto& req = std::get<GetLogicalNodeDirectoryRequest>(pdu.payload);
            Bytes fields;
            append(fields, explicit_string(0, req.logical_node_reference));
            append(fields, explicit_enum(1, static_cast<std::uint8_t>(req.acsi_class)));
            if (req.continue_after) append(fields, explicit_string(2, *req.continue_after));
            return encode_request_envelope(pdu, 2, fields);
        }
        if (pdu.service == ServiceKind::GetDataDirectory) {
            const auto& req = std::get<GetDataDirectoryRequest>(pdu.payload);
            Bytes fields;
            append(fields, explicit_string(0, req.data_reference));
            if (req.continue_after) append(fields, explicit_string(1, *req.continue_after));
            return encode_request_envelope(pdu, 5, fields);
        }
        if (pdu.service == ServiceKind::GetDataValues) {
            const auto& req = std::get<GetDataValuesRequest>(pdu.payload);
            Bytes fcd;
            append(fcd, explicit_string(0, req.ref.reference));
            append(fcd, explicit_enum(1, static_cast<std::uint8_t>(req.ref.fc)));
            const auto fcd_seq = sequence(fcd);
            Bytes fields;
            append(fields, context_explicit(0, fcd_seq));
            if (req.include_element_name) append(fields, explicit_bool(1, true));
            return encode_request_envelope(pdu, 3, fields);
        }
    }

    if (pdu.message_class == MessageClass::Response) {
        if (pdu.service == ServiceKind::ServiceError) {
            if (!pdu.invoke_id) throw Error("DMS BER: serviceError response missing invokeId");
            const auto& err = std::get<ServiceError>(pdu.payload);
            Bytes seq_body;
            append(seq_body, explicit_string(0, pdu.associate_id));
            append(seq_body, context_implicit_integer(1, *pdu.invoke_id));
            const auto status = enumerated(static_cast<std::uint8_t>(err.status));
            const auto choice = context_explicit(0, status);
            append(seq_body, context_explicit(2, choice));
            const auto seq = sequence(seq_body);
            return context_explicit(2, seq);
        }
        if (pdu.service == ServiceKind::GetServerDirectory) {
            const auto& rsp = std::get<GetServerDirectoryResponse>(pdu.payload);
            Bytes fields;
            const auto list = encode_string_list(rsp.logical_devices);
            append(fields, context_explicit(0, list));
            if (rsp.more_follows) append(fields, explicit_bool(1, *rsp.more_follows));
            return encode_response_envelope(pdu, 29, fields);
        }
        if (pdu.service == ServiceKind::GetLogicalDeviceDirectory) {
            const auto& rsp = std::get<GetLogicalDeviceDirectoryResponse>(pdu.payload);
            Bytes fields;
            const auto list = encode_string_list(rsp.logical_nodes);
            append(fields, context_explicit(0, list));
            if (rsp.more_follows) append(fields, explicit_bool(1, *rsp.more_follows));
            return encode_response_envelope(pdu, 1, fields);
        }
        if (pdu.service == ServiceKind::GetLogicalNodeDirectory) {
            const auto& rsp = std::get<GetLogicalNodeDirectoryResponse>(pdu.payload);
            Bytes fields;
            const auto list = encode_string_list(rsp.instance_names);
            append(fields, context_explicit(0, list));
            if (rsp.more_follows) append(fields, explicit_bool(1, *rsp.more_follows));
            return encode_response_envelope(pdu, 2, fields);
        }
        if (pdu.service == ServiceKind::GetDataDirectory) {
            const auto& rsp = std::get<GetDataDirectoryResponse>(pdu.payload);
            Bytes fields;
            if (!rsp.sub_data_objects.empty()) {
                const auto list = encode_string_list(rsp.sub_data_objects);
                append(fields, context_explicit(0, list));
            }
            if (!rsp.data_attributes.empty()) {
                const auto list = encode_string_list(rsp.data_attributes);
                append(fields, context_explicit(1, list));
            }
            if (rsp.more_follows) append(fields, explicit_bool(2, *rsp.more_follows));
            return encode_response_envelope(pdu, 5, fields);
        }
    }

    throw ber::Error("DMS BER: encode service not implemented");
}

} // namespace ar61850::dms
