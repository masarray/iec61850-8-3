#include "protocol_internal.hpp"

namespace ar61850::dms {
using namespace detail;

DmsPdu ProtocolCodec::decode(std::span<const std::uint8_t> bytes) const {
    using namespace ber;
    const auto outer = read_one(bytes, limits_);
    if (outer.encoded_size != bytes.size()) throw Error("DMS BER: trailing bytes after TpaaPdu");

    DmsPdu pdu;
    if (outer.tag == Tag{TagClass::Context, true, 0}) {
        pdu.message_class = MessageClass::Association;
        const auto assoc_type = children(outer, limits_, 0);
        if (assoc_type.size() != 1) throw Error("DMS BER: invalid AssociateType");
        require_tag(assoc_type[0], TagClass::Context, true, 1, "AssociateType.service");
        const auto services = children(assoc_type[0], limits_, 1);
        if (services.size() != 1) throw Error("DMS BER: invalid AssociateServiceType");
        const auto& service = services[0];

        if (service.tag == Tag{TagClass::Context, true, 0}) {
            pdu.service = ServiceKind::Associate;
            const auto fields = service_fields(service, limits_, "associateRequest");
            AssociateRequest req;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    req.called_ap = decode_explicit_string(field, 0, limits_, "calledAP");
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    const auto value = decode_explicit_integer(field, 1, limits_, "maxMessageSize");
                    if (value > std::numeric_limits<std::uint32_t>::max()) throw Error("DMS BER: maxMessageSize overflow");
                    req.max_message_size = static_cast<std::uint32_t>(value);
                }
            }
            pdu.payload = std::move(req);
            return pdu;
        }

        if (service.tag == Tag{TagClass::Context, true, 1}) {
            pdu.service = ServiceKind::Associate;
            const auto fields = service_fields(service, limits_, "associateResponse");
            AssociateResponse rsp;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    rsp.max_message_size = static_cast<std::uint32_t>(
                        decode_explicit_integer(field, 0, limits_, "maxMessageSize"));
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    rsp.associate_id = decode_explicit_string(field, 1, limits_, "associateId");
                } else if (field.tag == Tag{TagClass::Context, true, 2}) {
                    rsp.max_outstanding_calls = static_cast<std::uint16_t>(
                        decode_explicit_integer(field, 2, limits_, "maxOutstandingCalls"));
                } else if (field.tag == Tag{TagClass::Context, true, 3}) {
                    rsp.service_error = static_cast<ServiceStatus>(
                        decode_explicit_enum(field, 3, limits_, "serviceError"));
                }
            }
            pdu.associate_id = rsp.associate_id;
            pdu.payload = std::move(rsp);
            return pdu;
        }

        if (service.tag.tag_class == TagClass::Context && service.tag.constructed &&
            (service.tag.number == 2 || service.tag.number == 3 ||
             service.tag.number == 4 || service.tag.number == 5)) {
            const auto fields = service_fields(service, limits_, "association lifecycle");
            if (fields.size() < 2) throw Error("DMS BER: incomplete release/abort PDU");

            require_tag(fields[0], TagClass::Context, false, 0, "invokeId");
            const auto invoke = decode_unsigned_integer(fields[0]);
            if (invoke > std::numeric_limits<std::uint32_t>::max()) {
                throw Error("DMS BER: lifecycle invokeId overflow");
            }

            pdu.invoke_id = static_cast<std::uint32_t>(invoke);
            pdu.associate_id = decode_explicit_string(fields[1], 1, limits_, "associateId");
            pdu.service = (service.tag.number == 2 || service.tag.number == 3)
                ? ServiceKind::Release : ServiceKind::Abort;

            // monostate marks request, ServiceError{NoError} marks response.
            if (service.tag.number == 3 || service.tag.number == 5) {
                pdu.payload = ServiceError{ServiceStatus::NoError};
            }
            return pdu;
        }

        throw Error("DMS BER: unsupported associate service");
    }

    if (outer.tag == Tag{TagClass::Context, true, 1}) {
        pdu.message_class = MessageClass::Request;
        const auto env = decode_envelope(outer, 1, limits_);
        pdu.associate_id = env.associate_id;
        pdu.invoke_id = env.invoke_id;
        const auto& service = env.service_choice;

        if (service.tag == Tag{TagClass::Context, true, 29}) {
            pdu.service = ServiceKind::GetServerDirectory;
            const auto fields = service_fields(service, limits_, "getServerDirectory");
            GetServerDirectoryRequest req;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    req.object_class = static_cast<ObjectClass>(
                        decode_explicit_enum(field, 0, limits_, "objectClass"));
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    req.continue_after = decode_explicit_string(field, 1, limits_, "continueAfter");
                }
            }
            pdu.payload = std::move(req);
            return pdu;
        }

        if (service.tag == Tag{TagClass::Context, true, 1}) {
            pdu.service = ServiceKind::GetLogicalDeviceDirectory;
            const auto fields = service_fields(service, limits_, "getLogicalDeviceDirectory");
            GetLogicalDeviceDirectoryRequest req;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    req.logical_device = decode_explicit_string(field, 0, limits_, "ldName");
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    req.continue_after = decode_explicit_string(field, 1, limits_, "continueAfter");
                }
            }
            pdu.payload = std::move(req);
            return pdu;
        }

        if (service.tag == Tag{TagClass::Context, true, 2}) {
            pdu.service = ServiceKind::GetLogicalNodeDirectory;
            const auto fields = service_fields(service, limits_, "getLogicalNodeDirectory");
            GetLogicalNodeDirectoryRequest req;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    req.logical_node_reference = decode_explicit_string(field, 0, limits_, "lnRef");
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    req.acsi_class = static_cast<AcsiClass>(
                        decode_explicit_enum(field, 1, limits_, "aCSIClass"));
                } else if (field.tag == Tag{TagClass::Context, true, 2}) {
                    req.continue_after = decode_explicit_string(field, 2, limits_, "continueAfter");
                }
            }
            pdu.payload = std::move(req);
            return pdu;
        }

        if (service.tag == Tag{TagClass::Context, true, 5}) {
            pdu.service = ServiceKind::GetDataDirectory;
            const auto fields = service_fields(service, limits_, "getDataDirectory");
            GetDataDirectoryRequest req;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    req.data_reference = decode_explicit_string(field, 0, limits_, "dataRef");
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    req.continue_after = decode_explicit_string(field, 1, limits_, "continueAfter");
                }
            }
            pdu.payload = std::move(req);
            return pdu;
        }

        if (service.tag == Tag{TagClass::Context, true, 3}) {
            pdu.service = ServiceKind::GetDataValues;
            const auto fields = service_fields(service, limits_, "getDataValues");
            GetDataValuesRequest req;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    const auto fcd = unwrap_explicit(field, 0, limits_, "ref");
                    require_tag(fcd, TagClass::Universal, true, 16, "FcdFcdaType");
                    for (const auto& ff : children(fcd, limits_, 4)) {
                        if (ff.tag == Tag{TagClass::Context, true, 0}) {
                            req.ref.reference = decode_explicit_string(ff, 0, limits_, "ref.ref");
                        } else if (ff.tag == Tag{TagClass::Context, true, 1}) {
                            req.ref.fc = static_cast<FunctionalConstraint>(
                                decode_explicit_enum(ff, 1, limits_, "ref.fc"));
                        }
                    }
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    req.include_element_name = decode_explicit_bool(
                        field, 1, limits_, "includeElementName");
                }
            }
            pdu.payload = std::move(req);
            return pdu;
        }

        pdu.service = ServiceKind::Unknown;
        return pdu;
    }

    if (outer.tag == Tag{TagClass::Context, true, 2}) {
        pdu.message_class = MessageClass::Response;
        const auto env = decode_envelope(outer, 2, limits_);
        pdu.associate_id = env.associate_id;
        pdu.invoke_id = env.invoke_id;
        const auto& service = env.service_choice;

        if (service.tag == Tag{TagClass::Context, true, 0}) {
            pdu.service = ServiceKind::ServiceError;
            const auto status = unwrap_explicit(service, 0, limits_, "serviceError");
            require_tag(status, TagClass::Universal, false, 10, "serviceError");
            pdu.payload = ServiceError{
                static_cast<ServiceStatus>(decode_unsigned_integer(status))
            };
            return pdu;
        }

        if (service.tag == Tag{TagClass::Context, true, 29}) {
            pdu.service = ServiceKind::GetServerDirectory;
            const auto fields = service_fields(service, limits_, "getServerDirectoryResponse");
            GetServerDirectoryResponse rsp;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    rsp.logical_devices = decode_string_list(field, 0, limits_, "result");
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    rsp.more_follows = decode_explicit_bool(field, 1, limits_, "moreFollows");
                }
            }
            pdu.payload = std::move(rsp);
            return pdu;
        }

        if (service.tag == Tag{TagClass::Context, true, 1}) {
            pdu.service = ServiceKind::GetLogicalDeviceDirectory;
            const auto fields = service_fields(service, limits_, "getLogicalDeviceDirectoryResponse");
            GetLogicalDeviceDirectoryResponse rsp;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    rsp.logical_nodes = decode_string_list(field, 0, limits_, "lnRef");
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    rsp.more_follows = decode_explicit_bool(field, 1, limits_, "moreFollows");
                }
            }
            pdu.payload = std::move(rsp);
            return pdu;
        }

        if (service.tag == Tag{TagClass::Context, true, 2}) {
            pdu.service = ServiceKind::GetLogicalNodeDirectory;
            const auto fields = service_fields(service, limits_, "getLogicalNodeDirectoryResponse");
            GetLogicalNodeDirectoryResponse rsp;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    rsp.instance_names = decode_string_list(field, 0, limits_, "instanceNames");
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    rsp.more_follows = decode_explicit_bool(field, 1, limits_, "moreFollows");
                }
            }
            pdu.payload = std::move(rsp);
            return pdu;
        }

        if (service.tag == Tag{TagClass::Context, true, 5}) {
            pdu.service = ServiceKind::GetDataDirectory;
            const auto fields = service_fields(service, limits_, "getDataDirectoryResponse");
            GetDataDirectoryResponse rsp;
            for (const auto& field : fields) {
                if (field.tag == Tag{TagClass::Context, true, 0}) {
                    rsp.sub_data_objects = decode_string_list(
                        field, 0, limits_, "subDataObjectName");
                } else if (field.tag == Tag{TagClass::Context, true, 1}) {
                    rsp.data_attributes = decode_string_list(
                        field, 1, limits_, "dataAttrName");
                } else if (field.tag == Tag{TagClass::Context, true, 2}) {
                    rsp.more_follows = decode_explicit_bool(
                        field, 2, limits_, "moreFollows");
                }
            }
            pdu.payload = std::move(rsp);
            return pdu;
        }

        pdu.service = ServiceKind::Unknown;
        return pdu;
    }

    throw Error("DMS BER: unsupported TpaaPdu class");
}

} // namespace ar61850::dms
