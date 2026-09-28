#include "ar61850/dms/ber.hpp"
#include "ar61850/dms/protocol.hpp"
#include "ar61850/dms/server_core.hpp"

#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

using namespace ar61850::dms;

static ber::Bytes hex(std::string_view text) {
    ber::Bytes out;
    unsigned value = 0;
    int nibble = 0;
    for (const char c : text) {
        if (c == ' ' || c == '\n' || c == '\t') continue;
        unsigned x;
        if (c >= '0' && c <= '9') x = c - '0';
        else if (c >= 'a' && c <= 'f') x = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') x = c - 'A' + 10;
        else assert(false);
        value = (value << 4) | x;
        if (++nibble == 2) {
            out.push_back(static_cast<std::uint8_t>(value));
            value = 0;
            nibble = 0;
        }
    }
    assert(nibble == 0);
    return out;
}

static void assert_equal(const ber::Bytes& a, const ber::Bytes& b, const char* what) {
    if (a == b) return;
    std::cerr << what << " mismatch\nactual:   ";
    for (auto x : a) std::cerr << std::hex << std::setw(2) << std::setfill('0') << unsigned(x);
    std::cerr << "\nexpected: ";
    for (auto x : b) std::cerr << std::hex << std::setw(2) << std::setfill('0') << unsigned(x);
    std::cerr << "\n";
    std::abort();
}

int main() {
    ProtocolCodec codec;

    // Captured from the pinned Netbeheer FT20 reference behavior.
    const auto associate_rsp = hex(
        "a01ca11aa1183016a005020300fde8a1080c0669645f637031a20302010a");
    auto p = codec.decode(associate_rsp);
    assert(p.message_class == MessageClass::Association);
    assert(p.service == ServiceKind::Associate);
    const auto& ar = std::get<AssociateResponse>(p.payload);
    assert(ar.max_message_size == 65000);
    assert(ar.associate_id == "id_cp1");
    assert(ar.max_outstanding_calls && *ar.max_outstanding_calls == 10);
    assert_equal(codec.encode(p), associate_rsp, "associateResponse");

    const auto server_dir_rsp = hex(
        "a21e301ca0080c0669645f637031810100a20dbd0b3009a00730050c034c4430");
    p = codec.decode(server_dir_rsp);
    assert(p.service == ServiceKind::GetServerDirectory);
    assert(p.invoke_id && *p.invoke_id == 0);
    const auto& sd = std::get<GetServerDirectoryResponse>(p.payload);
    assert(sd.logical_devices.size() == 1 && sd.logical_devices[0] == "LD0");
    assert_equal(codec.encode(p), server_dir_rsp, "getServerDirectory response");

    const auto ld_dir_rsp = hex(
        "a23b3039a0080c0669645f637031810101a22aa1283026a02430220c044c4c4e30"
        "0c054c504844310c0544574d58310c054447454e310c054d4d585531");
    p = codec.decode(ld_dir_rsp);
    assert(p.service == ServiceKind::GetLogicalDeviceDirectory);
    const auto& ld = std::get<GetLogicalDeviceDirectoryResponse>(p.payload);
    assert(ld.logical_nodes.size() == 5);
    assert(ld.logical_nodes.front() == "LLN0");
    assert(ld.logical_nodes.back() == "MMXU1");
    assert_equal(codec.encode(p), ld_dir_rsp, "getLogicalDeviceDirectory response");

    const auto ln_dir_rsp = hex(
        "a2333031a0080c0669645f637031810102a222a220301ea01c301a0c034d6f640c"
        "064e616d506c740c034265680c064865616c7468");
    p = codec.decode(ln_dir_rsp);
    assert(p.service == ServiceKind::GetLogicalNodeDirectory);
    const auto& ln = std::get<GetLogicalNodeDirectoryResponse>(p.payload);
    assert(ln.instance_names.size() == 4);
    assert(ln.instance_names[0] == "Mod");
    assert(ln.instance_names[3] == "Health");
    assert_equal(codec.encode(p), ln_dir_rsp, "getLogicalNodeDirectory response");

    DmsPdu req;
    req.message_class = MessageClass::Request;
    req.service = ServiceKind::GetServerDirectory;
    req.associate_id = "id_cp1";
    req.invoke_id = 7;
    req.payload = GetServerDirectoryRequest{};
    const auto encoded_req = codec.encode(req);
    const auto decoded_req = codec.decode(encoded_req);
    assert(decoded_req.service == ServiceKind::GetServerDirectory);
    assert(decoded_req.invoke_id && *decoded_req.invoke_id == 7);

    // Native standalone server core: Netbeheer isn't loaded or spawned.
    ServerCore server{IedModel::make_ft20_reference_model()};
    DmsPdu assoc;
    assoc.message_class = MessageClass::Association;
    assoc.service = ServiceKind::Associate;
    assoc.payload = AssociateRequest{
        .called_ap = std::string("cp1"),
        .max_message_size = 65000
    };
    auto wire_rsp = server.handle(codec.encode(assoc));
    assert(wire_rsp.has_value());
    assert_equal(*wire_rsp, associate_rsp, "native server associate response");
    assert(server.associated());
    assert(server.associate_id() == "id_cp1");

    DmsPdu native_req;
    native_req.message_class = MessageClass::Request;
    native_req.service = ServiceKind::GetServerDirectory;
    native_req.associate_id = "id_cp1";
    native_req.invoke_id = 0;
    native_req.payload = GetServerDirectoryRequest{};
    wire_rsp = server.handle(codec.encode(native_req));
    assert(wire_rsp.has_value());
    assert_equal(*wire_rsp, server_dir_rsp, "native server GetServerDirectory");

    native_req.service = ServiceKind::GetLogicalDeviceDirectory;
    native_req.invoke_id = 1;
    native_req.payload = GetLogicalDeviceDirectoryRequest{.logical_device = "LD0"};
    wire_rsp = server.handle(codec.encode(native_req));
    assert(wire_rsp.has_value());
    assert_equal(*wire_rsp, ld_dir_rsp, "native server GetLogicalDeviceDirectory");

    native_req.service = ServiceKind::GetLogicalNodeDirectory;
    native_req.invoke_id = 2;
    native_req.payload = GetLogicalNodeDirectoryRequest{
        .logical_node_reference = "LD0/LLN0",
        .acsi_class = AcsiClass::DataObject
    };
    wire_rsp = server.handle(codec.encode(native_req));
    assert(wire_rsp.has_value());
    assert_equal(*wire_rsp, ln_dir_rsp, "native server GetLogicalNodeDirectory");

    // P2 data-definition and live-value services are native and typed.
    native_req.service = ServiceKind::GetDataDefinition;
    native_req.invoke_id = 3;
    native_req.payload = GetDataDefinitionRequest{
        .data_reference = "LD0/MMXU1.TotW"
    };
    wire_rsp = server.handle(codec.encode(native_req));
    assert(wire_rsp.has_value());
    auto data_definition_pdu = codec.decode(*wire_rsp);
    assert(data_definition_pdu.service == ServiceKind::GetDataDefinition);
    const auto& data_definition =
        std::get<GetDataDefinitionResponse>(data_definition_pdu.payload);
    assert(data_definition.cdc && *data_definition.cdc == "MV");
    assert(data_definition.data_attributes.size() == 4);
    assert(data_definition.data_attributes[0].reference == "mag");
    assert(data_definition.data_attributes[0].type == DataType::Structure);
    assert(data_definition.data_attributes[0].components.size() == 1);
    assert(data_definition.data_attributes[0].components[0].reference == "f");
    assert(data_definition.data_attributes[0].components[0].type == DataType::Float32);

    native_req.service = ServiceKind::GetDataValues;
    native_req.invoke_id = 4;
    native_req.payload = GetDataValuesRequest{
        .ref = FcdFcdaRef{
            .reference = "LD0/MMXU1.TotW",
            .fc = FunctionalConstraint::MX
        },
        .include_element_name = true
    };
    wire_rsp = server.handle(codec.encode(native_req));
    assert(wire_rsp.has_value());
    auto values_pdu = codec.decode(*wire_rsp);
    assert(values_pdu.service == ServiceKind::GetDataValues);
    const auto& values = std::get<GetDataValuesResponse>(values_pdu.payload);
    assert(values.data_attribute_values.size() == 3);
    assert(values.data_attribute_values[0].name == "mag");
    assert(values.data_attribute_values[0].type == DataType::Structure);
    assert(values.data_attribute_values[0].children.size() == 1);
    assert(values.data_attribute_values[0].children[0].type == DataType::Float32);
    assert(std::get<float>(values.data_attribute_values[0].children[0].scalar) == 10.0F);
    assert(values.data_attribute_values[1].name == "q");
    assert(values.data_attribute_values[1].type == DataType::Quality);
    assert(values.data_attribute_values[2].name == "t");
    assert(values.data_attribute_values[2].type == DataType::Timestamp);

    // Direct FCDA reads return the leaf value, not a synthetic polling fallback.
    native_req.invoke_id = 5;
    native_req.payload = GetDataValuesRequest{
        .ref = FcdFcdaRef{
            .reference = "LD0/MMXU1.TotW.mag.f",
            .fc = FunctionalConstraint::MX
        },
        .include_element_name = true
    };
    wire_rsp = server.handle(codec.encode(native_req));
    assert(wire_rsp.has_value());
    values_pdu = codec.decode(*wire_rsp);
    const auto& leaf_values = std::get<GetDataValuesResponse>(values_pdu.payload);
    assert(leaf_values.data_attribute_values.size() == 1);
    assert(leaf_values.data_attribute_values[0].name == "f");
    assert(leaf_values.data_attribute_values[0].type == DataType::Float32);
    assert(std::get<float>(leaf_values.data_attribute_values[0].scalar) == 10.0F);

    // P4 DataSet foundation: static directory and values preserve member order.
    native_req.service = ServiceKind::GetLogicalNodeDirectory;
    native_req.invoke_id = 6;
    native_req.payload = GetLogicalNodeDirectoryRequest{
        .logical_node_reference = "LD0/LLN0",
        .acsi_class = AcsiClass::DataSet
    };
    wire_rsp = server.handle(codec.encode(native_req));
    assert(wire_rsp.has_value());
    auto dataset_list_pdu = codec.decode(*wire_rsp);
    const auto& dataset_list =
        std::get<GetLogicalNodeDirectoryResponse>(dataset_list_pdu.payload);
    assert(dataset_list.instance_names.size() == 3);
    assert(dataset_list.instance_names[0] == "DataSetMinMaxAvg");
    assert(dataset_list.instance_names[2] == "DataSetActualValues");

    native_req.service = ServiceKind::GetDataSetDirectory;
    native_req.invoke_id = 7;
    native_req.payload = GetDataSetDirectoryRequest{
        .data_set_reference = "LD0/LLN0.DataSetActualValues"
    };
    wire_rsp = server.handle(codec.encode(native_req));
    assert(wire_rsp.has_value());
    auto dataset_directory_pdu = codec.decode(*wire_rsp);
    assert(dataset_directory_pdu.service == ServiceKind::GetDataSetDirectory);
    const auto& dataset_directory =
        std::get<GetDataSetDirectoryResponse>(dataset_directory_pdu.payload);
    assert(dataset_directory.members.size() == 11);
    assert(dataset_directory.members[0].reference == "LD0/MMXU1.TotW");
    assert(dataset_directory.members[0].fc == FunctionalConstraint::MX);
    assert(dataset_directory.members[10].reference == "LD0/MMXU1.A.phsC");

    native_req.service = ServiceKind::GetDataSetValues;
    native_req.invoke_id = 8;
    native_req.payload = GetDataSetValuesRequest{
        .data_set_reference = "LD0/LLN0.DataSetActualValues"
    };
    wire_rsp = server.handle(codec.encode(native_req));
    assert(wire_rsp.has_value());
    auto dataset_values_pdu = codec.decode(*wire_rsp);
    assert(dataset_values_pdu.service == ServiceKind::GetDataSetValues);
    const auto& dataset_values =
        std::get<GetDataSetValuesResponse>(dataset_values_pdu.payload);
    assert(dataset_values.member_values.size() == 11);
    assert(dataset_values.member_values[0].type == DataType::Structure);
    assert(dataset_values.member_values[0].children.size() == 3);
    assert(dataset_values.member_values[0].children[0].type == DataType::Structure);
    assert(dataset_values.member_values[0].children[0].children.size() == 1);
    assert(std::get<float>(
        dataset_values.member_values[0].children[0].children[0].scalar) == 10.0F);

    // Canonical model exposes nested IEC attributes without dynamic dictionaries.
    {
        auto model = IedModel::make_ft20_reference_model();
        const auto* totw = model.find_data_object("LD0/MMXU1.TotW");
        assert(totw && totw->cdc == "MV");

        const auto* mag_f = model.find_data_attribute("LD0/MMXU1.TotW.mag.f");
        assert(mag_f && mag_f->fc == FunctionalConstraint::MX);
        assert(mag_f->type == DataType::Float32);
        assert(std::get<float>(mag_f->value) == 10.0F);

        assert(model.set_float("LD0/MMXU1.TotW.mag.f", 42.5F));
        mag_f = model.find_data_attribute("LD0/MMXU1.TotW.mag.f");
        assert(mag_f && std::get<float>(mag_f->value) == 42.5F);

        const auto* phase = model.find_data_object("LD0/MMXU1.PhV.phsA");
        assert(phase && phase->cdc == "CMV");
        const auto* phase_mag = model.find_data_attribute("LD0/MMXU1.PhV.phsA.cVal.mag.f");
        assert(phase_mag && phase_mag->type == DataType::Float32);
    }

    // Association lifecycle is explicit and leaves the server disconnected.
    DmsPdu release_req;
    release_req.message_class = MessageClass::Association;
    release_req.service = ServiceKind::Release;
    release_req.associate_id = "id_cp1";
    release_req.invoke_id = 9;

    const auto release_wire = codec.encode(release_req);
    const auto release_decoded = codec.decode(release_wire);
    assert(release_decoded.service == ServiceKind::Release);
    assert(release_decoded.invoke_id && *release_decoded.invoke_id == 9);
    assert(release_decoded.associate_id == "id_cp1");

    wire_rsp = server.handle(release_wire);
    assert(wire_rsp.has_value());
    const auto release_rsp = codec.decode(*wire_rsp);
    assert(release_rsp.service == ServiceKind::Release);
    assert(release_rsp.invoke_id && *release_rsp.invoke_id == 9);
    assert(!server.associated());

    // Re-associate and test abort independently.
    wire_rsp = server.handle(codec.encode(assoc));
    assert(wire_rsp.has_value() && server.associated());

    DmsPdu abort_req;
    abort_req.message_class = MessageClass::Association;
    abort_req.service = ServiceKind::Abort;
    abort_req.associate_id = "id_cp1";
    abort_req.invoke_id = 10;

    wire_rsp = server.handle(codec.encode(abort_req));
    assert(wire_rsp.has_value());
    const auto abort_rsp = codec.decode(*wire_rsp);
    assert(abort_rsp.service == ServiceKind::Abort);
    assert(abort_rsp.invoke_id && *abort_rsp.invoke_id == 10);
    assert(!server.associated());

    // Hardening: reject indefinite length and truncation.
    bool rejected = false;
    try { codec.decode(hex("a0800000")); } catch (const ber::Error&) { rejected = true; }
    assert(rejected);

    rejected = false;
    try { codec.decode(hex("a2053003a0")); } catch (const ber::Error&) { rejected = true; }
    assert(rejected);

    std::cout << "protocol tests passed\n";
    return 0;
}
