#include "ar61850/dms/model.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace ar61850::dms {
namespace {

std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (const char c : value) {
        if (c == delimiter) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

Quality default_quality() {
    return Quality{
        .validity = Validity::Good,
        .source = Source::Process,
        .test = false,
        .operator_blocked = false
    };
}

Timestamp deterministic_timestamp() {
    Timestamp t;
    t.value = std::chrono::system_clock::time_point{std::chrono::seconds{1720458123}}
        + std::chrono::microseconds{123456};
    t.clock_failure = false;
    t.clock_not_synchronized = false;
    t.time_accuracy = static_cast<std::uint8_t>(3);
    return t;
}

DataAttributeNode leaf(
    std::string name,
    FunctionalConstraint fc,
    DataType type,
    DataScalar value = {}) {
    return DataAttributeNode{
        .name = std::move(name),
        .fc = fc,
        .type = type,
        .value = std::move(value),
        .children = {}
    };
}

DataAttributeNode structure(
    std::string name,
    FunctionalConstraint fc,
    std::vector<DataAttributeNode> children) {
    return DataAttributeNode{
        .name = std::move(name),
        .fc = fc,
        .type = DataType::Structure,
        .value = std::monostate{},
        .children = std::move(children)
    };
}

DataAttributeNode units() {
    return structure("units", FunctionalConstraint::CF, {
        leaf("SIUnit", FunctionalConstraint::CF, DataType::Enumerated, std::uint64_t{0}),
        leaf("multiplier", FunctionalConstraint::CF, DataType::Enumerated, std::uint64_t{0})
    });
}

DataObjectNode mv(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "MV",
        .children = {},
        .attributes = {
            structure("mag", FunctionalConstraint::MX, {
                leaf("f", FunctionalConstraint::MX, DataType::Float32, 0.0F)
            }),
            leaf("q", FunctionalConstraint::MX, DataType::Quality, default_quality()),
            leaf("t", FunctionalConstraint::MX, DataType::Timestamp, deterministic_timestamp()),
            units()
        }
    };
}

DataObjectNode ens(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "ENS",
        .children = {},
        .attributes = {
            leaf("stVal", FunctionalConstraint::ST, DataType::Enumerated, std::uint64_t{0}),
            leaf("q", FunctionalConstraint::ST, DataType::Quality, default_quality()),
            leaf("t", FunctionalConstraint::ST, DataType::Timestamp, deterministic_timestamp())
        }
    };
}

DataObjectNode enc(std::string name) {
    auto object = ens(std::move(name));
    object.cdc = "ENC";
    object.attributes.push_back(
        leaf("ctlModel", FunctionalConstraint::CF, DataType::Enumerated, std::uint64_t{0}));
    return object;
}

DataObjectNode sps(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "SPS",
        .children = {},
        .attributes = {
            leaf("stVal", FunctionalConstraint::ST, DataType::Boolean, false),
            leaf("q", FunctionalConstraint::ST, DataType::Quality, default_quality()),
            leaf("t", FunctionalConstraint::ST, DataType::Timestamp, deterministic_timestamp())
        }
    };
}

DataObjectNode lpl(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "LPL",
        .children = {},
        .attributes = {
            leaf("vendor", FunctionalConstraint::DC, DataType::VisibleString255, std::string{}),
            leaf("swRev", FunctionalConstraint::DC, DataType::VisibleString255, std::string{}),
            leaf("configRev", FunctionalConstraint::DC, DataType::VisibleString255, std::string{}),
            leaf("lnNs", FunctionalConstraint::EX, DataType::VisibleString255, std::string{})
        }
    };
}

DataObjectNode dpl(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "DPL",
        .children = {},
        .attributes = {
            leaf("vendor", FunctionalConstraint::DC, DataType::VisibleString255, std::string{}),
            leaf("hwRev", FunctionalConstraint::DC, DataType::VisibleString255, std::string{}),
            leaf("swRev", FunctionalConstraint::DC, DataType::VisibleString255, std::string{}),
            leaf("serNum", FunctionalConstraint::DC, DataType::VisibleString255, std::string{}),
            leaf("model", FunctionalConstraint::DC, DataType::VisibleString255, std::string{}),
            leaf("location", FunctionalConstraint::DC, DataType::VisibleString255, std::string{})
        }
    };
}

DataObjectNode cmv(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "CMV",
        .children = {},
        .attributes = {
            structure("cVal", FunctionalConstraint::MX, {
                structure("mag", FunctionalConstraint::MX, {
                    leaf("f", FunctionalConstraint::MX, DataType::Float32, 0.0F)
                })
            }),
            leaf("q", FunctionalConstraint::MX, DataType::Quality, default_quality()),
            leaf("t", FunctionalConstraint::MX, DataType::Timestamp, deterministic_timestamp()),
            units()
        }
    };
}

DataObjectNode wye(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "WYE",
        .children = {cmv("phsA"), cmv("phsB"), cmv("phsC")},
        .attributes = {}
    };
}

DataObjectNode del(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "DEL",
        .children = {cmv("phsAB"), cmv("phsBC"), cmv("phsCA")},
        .attributes = {}
    };
}

DataObjectNode asg(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "ASG",
        .children = {},
        .attributes = {
            structure("setMag", FunctionalConstraint::SP, {
                leaf("f", FunctionalConstraint::SP, DataType::Float32, 0.0F)
            }),
            units(),
            leaf("dataNs", FunctionalConstraint::EX, DataType::VisibleString255, std::string{})
        }
    };
}

DataObjectNode ing(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "ING",
        .children = {},
        .attributes = {
            leaf("setVal", FunctionalConstraint::SP, DataType::Int32, std::int64_t{0}),
            units(),
            leaf("dataNs", FunctionalConstraint::EX, DataType::VisibleString255, std::string{})
        }
    };
}

DataObjectNode apc(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "APC",
        .children = {},
        .attributes = {
            structure("Oper", FunctionalConstraint::CO, {
                structure("ctlVal", FunctionalConstraint::CO, {
                    leaf("f", FunctionalConstraint::CO, DataType::Float32, 0.0F)
                }),
                structure("origin", FunctionalConstraint::CO, {
                    leaf("orCat", FunctionalConstraint::CO, DataType::Enumerated, std::uint64_t{0}),
                    leaf("orIdent", FunctionalConstraint::CO, DataType::OctetString, Bytes{})
                }),
                leaf("ctlNum", FunctionalConstraint::CO, DataType::UInt8, std::uint64_t{0}),
                leaf("T", FunctionalConstraint::CO, DataType::Timestamp, deterministic_timestamp()),
                leaf("Test", FunctionalConstraint::CO, DataType::Boolean, false),
                structure("Check", FunctionalConstraint::CO, {
                    leaf("synchroCheck", FunctionalConstraint::CO, DataType::Boolean, false),
                    leaf("interlockCheck", FunctionalConstraint::CO, DataType::Boolean, false)
                })
            }),
            structure("mxVal", FunctionalConstraint::MX, {
                leaf("f", FunctionalConstraint::MX, DataType::Float32, 0.0F)
            }),
            leaf("q", FunctionalConstraint::MX, DataType::Quality, default_quality()),
            leaf("t", FunctionalConstraint::MX, DataType::Timestamp, deterministic_timestamp()),
            units(),
            leaf("ctlModel", FunctionalConstraint::CF, DataType::Enumerated, std::uint64_t{1})
        }
    };
}

DataObjectNode inc(std::string name) {
    return DataObjectNode{
        .name = std::move(name),
        .cdc = "INC",
        .children = {},
        .attributes = {
            structure("Oper", FunctionalConstraint::CO, {
                structure("ctlVal", FunctionalConstraint::CO, {
                    leaf("f", FunctionalConstraint::CO, DataType::Float32, 0.0F)
                })
            }),
            leaf("stVal", FunctionalConstraint::ST, DataType::Int32, std::int64_t{0}),
            leaf("q", FunctionalConstraint::ST, DataType::Quality, default_quality()),
            leaf("t", FunctionalConstraint::ST, DataType::Timestamp, deterministic_timestamp()),
            leaf("ctlModel", FunctionalConstraint::CF, DataType::Enumerated, std::uint64_t{1}),
            leaf("dataNs", FunctionalConstraint::EX, DataType::VisibleString255, std::string{})
        }
    };
}

const DataAttributeNode* find_attr(
    const std::vector<DataAttributeNode>& attributes,
    const std::vector<std::string>& path,
    std::size_t index) noexcept {
    if (index >= path.size()) return nullptr;
    const auto it = std::find_if(attributes.begin(), attributes.end(),
        [&](const auto& item) { return item.name == path[index]; });
    if (it == attributes.end()) return nullptr;
    if (index + 1 == path.size()) return &*it;
    return find_attr(it->children, path, index + 1);
}

DataAttributeNode* find_attr(
    std::vector<DataAttributeNode>& attributes,
    const std::vector<std::string>& path,
    std::size_t index) noexcept {
    if (index >= path.size()) return nullptr;
    const auto it = std::find_if(attributes.begin(), attributes.end(),
        [&](const auto& item) { return item.name == path[index]; });
    if (it == attributes.end()) return nullptr;
    if (index + 1 == path.size()) return &*it;
    return find_attr(it->children, path, index + 1);
}

template <typename Object>
Object* descend_object(Object* current, const std::vector<std::string>& path, std::size_t& index) noexcept {
    while (current && index < path.size()) {
        auto it = std::find_if(current->children.begin(), current->children.end(),
            [&](const auto& child) { return child.name == path[index]; });
        if (it == current->children.end()) break;
        current = &*it;
        ++index;
    }
    return current;
}

} // namespace

IedModel::IedModel(std::string ied_name) : ied_name_(std::move(ied_name)) {}

const LogicalDeviceModel* IedModel::find_logical_device(std::string_view name) const noexcept {
    const auto it = std::find_if(logical_devices_.begin(), logical_devices_.end(),
        [name](const auto& item) { return item.name == name; });
    return it == logical_devices_.end() ? nullptr : &*it;
}

const LogicalNodeModel* IedModel::find_logical_node(std::string_view reference) const noexcept {
    const auto slash = reference.find('/');
    if (slash == std::string_view::npos) return nullptr;
    const auto* ld = find_logical_device(reference.substr(0, slash));
    if (!ld) return nullptr;
    const auto ln_name = reference.substr(slash + 1);
    const auto it = std::find_if(ld->logical_nodes.begin(), ld->logical_nodes.end(),
        [ln_name](const auto& item) { return item.name == ln_name; });
    return it == ld->logical_nodes.end() ? nullptr : &*it;
}

const DataObjectNode* IedModel::find_data_object(std::string_view reference) const noexcept {
    const auto dot = reference.find('.');
    if (dot == std::string_view::npos) return nullptr;
    const auto* ln = find_logical_node(reference.substr(0, dot));
    if (!ln) return nullptr;
    const auto path = split(reference.substr(dot + 1), '.');
    if (path.empty()) return nullptr;

    auto first = std::find_if(ln->data_objects.begin(), ln->data_objects.end(),
        [&](const auto& item) { return item.name == path[0]; });
    if (first == ln->data_objects.end()) return nullptr;

    const DataObjectNode* current = &*first;
    std::size_t index = 1;
    current = descend_object(current, path, index);
    return current && index == path.size() ? current : nullptr;
}

const DataAttributeNode* IedModel::find_data_attribute(std::string_view reference) const noexcept {
    const auto dot = reference.find('.');
    if (dot == std::string_view::npos) return nullptr;
    const auto* ln = find_logical_node(reference.substr(0, dot));
    if (!ln) return nullptr;

    const auto path = split(reference.substr(dot + 1), '.');
    if (path.size() < 2) return nullptr;

    auto first = std::find_if(ln->data_objects.begin(), ln->data_objects.end(),
        [&](const auto& item) { return item.name == path[0]; });
    if (first == ln->data_objects.end()) return nullptr;

    const DataObjectNode* object = &*first;
    std::size_t index = 1;
    object = descend_object(object, path, index);
    if (!object || index >= path.size()) return nullptr;
    return find_attr(object->attributes, path, index);
}

const DataSetModel* IedModel::find_data_set(std::string_view reference) const noexcept {
    const auto dot = reference.find('.');
    if (dot == std::string_view::npos) return nullptr;
    const auto* ln = find_logical_node(reference.substr(0, dot));
    if (!ln) return nullptr;
    const auto name = reference.substr(dot + 1);
    const auto it = std::find_if(
        ln->data_sets.begin(), ln->data_sets.end(),
        [&](const auto& data_set) {
            return data_set.name == name || data_set.reference == reference;
        });
    return it == ln->data_sets.end() ? nullptr : &*it;
}


const ReportControlState* IedModel::find_report_control(
    std::string_view reference) const noexcept {
    const auto dot = reference.find('.');
    if (dot == std::string_view::npos) return nullptr;
    const auto* ln = find_logical_node(reference.substr(0, dot));
    if (!ln) return nullptr;
    const auto name = reference.substr(dot + 1);
    const auto it = std::find_if(
        ln->report_controls.begin(), ln->report_controls.end(),
        [&](const auto& rcb) {
            return rcb.reference.value == reference ||
                rcb.reference.value == std::string(reference.substr(0, dot + 1)) + std::string(name);
        });
    return it == ln->report_controls.end() ? nullptr : &*it;
}

ReportControlState* IedModel::find_report_control(
    std::string_view reference) noexcept {
    const auto dot = reference.find('.');
    const auto slash = reference.find('/');
    if (dot == std::string_view::npos ||
        slash == std::string_view::npos || slash > dot) return nullptr;
    const auto ld_name = reference.substr(0, slash);
    const auto ln_name = reference.substr(slash + 1, dot - slash - 1);

    auto ld = std::find_if(
        logical_devices_.begin(), logical_devices_.end(),
        [&](const auto& item) { return item.name == ld_name; });
    if (ld == logical_devices_.end()) return nullptr;

    auto ln = std::find_if(
        ld->logical_nodes.begin(), ld->logical_nodes.end(),
        [&](const auto& item) { return item.name == ln_name; });
    if (ln == ld->logical_nodes.end()) return nullptr;

    const auto it = std::find_if(
        ln->report_controls.begin(), ln->report_controls.end(),
        [&](const auto& rcb) { return rcb.reference.value == reference; });
    return it == ln->report_controls.end() ? nullptr : &*it;
}

DataAttributeNode* IedModel::find_data_attribute(std::string_view reference) noexcept {
    const auto dot = reference.find('.');
    if (dot == std::string_view::npos) return nullptr;

    const auto slash = reference.find('/');
    if (slash == std::string_view::npos || slash > dot) return nullptr;
    const auto ld_name = reference.substr(0, slash);
    const auto ln_name = reference.substr(slash + 1, dot - slash - 1);

    auto ld_it = std::find_if(logical_devices_.begin(), logical_devices_.end(),
        [&](const auto& item) { return item.name == ld_name; });
    if (ld_it == logical_devices_.end()) return nullptr;
    auto ln_it = std::find_if(ld_it->logical_nodes.begin(), ld_it->logical_nodes.end(),
        [&](const auto& item) { return item.name == ln_name; });
    if (ln_it == ld_it->logical_nodes.end()) return nullptr;

    const auto path = split(reference.substr(dot + 1), '.');
    if (path.size() < 2) return nullptr;

    auto first = std::find_if(ln_it->data_objects.begin(), ln_it->data_objects.end(),
        [&](const auto& item) { return item.name == path[0]; });
    if (first == ln_it->data_objects.end()) return nullptr;

    DataObjectNode* object = &*first;
    std::size_t index = 1;
    object = descend_object(object, path, index);
    if (!object || index >= path.size()) return nullptr;
    return find_attr(object->attributes, path, index);
}

bool IedModel::set_float(std::string_view reference, float value) noexcept {
    auto* attr = find_data_attribute(reference);
    if (!attr || attr->type != DataType::Float32) return false;
    attr->value = value;
    return true;
}

bool IedModel::set_float_batch(
    const std::vector<std::pair<std::string, float>>& updates) noexcept {
    std::vector<DataAttributeNode*> targets;
    targets.reserve(updates.size());

    for (const auto& [reference, value] : updates) {
        (void) value;
        auto* attr = find_data_attribute(reference);
        if (!attr || attr->type != DataType::Float32) return false;
        targets.push_back(attr);
    }

    for (std::size_t i = 0; i < updates.size(); ++i) {
        targets[i]->value = updates[i].second;
    }
    return true;
}

bool IedModel::set_quality(
    std::string_view reference,
    Quality quality) noexcept {
    auto* attr = find_data_attribute(reference);
    if (!attr || attr->type != DataType::Quality) return false;
    attr->value = quality;
    return true;
}


IedModel IedModel::make_ft20_reference_model() {
    IedModel model{"IED1"};

    LogicalDeviceModel ld0;
    ld0.name = "LD0";

    LogicalNodeModel lln0{"LLN0", {
        enc("Mod"), lpl("NamPlt"), ens("Beh"), ens("Health")
    }};

    LogicalNodeModel lphd1{"LPHD1", {
        dpl("PhyNam"), ens("PhyHealth"), sps("Proxy"), sps("PwrUp")
    }};

    LogicalNodeModel dwmx1{"DWMX1", {
        lpl("NamPlt"), ens("Beh"), apc("WMaxSptPct"), apc("WMaxSpt"),
        asg("WMaxSetPct"), asg("WMaxSet"), ing("WMaxFto"), inc("SptReas")
    }};

    LogicalNodeModel dgen1{"DGEN1", {
        lpl("NamPlt"), ens("Beh"), ens("DEROpSt")
    }};

    LogicalNodeModel mmxu1{"MMXU1", {
        ens("Beh"), mv("TotW"), mv("TotVAr"), wye("PhV"), del("PPV"), wye("A"),
        mv("AvWPhs"), mv("MaxWPhs"), mv("MinWPhs")
    }};

    lln0.data_sets = {
        DataSetModel{
            .name = "DataSetMinMaxAvg",
            .reference = "LD0/LLN0.DataSetMinMaxAvg",
            .members = {
                {"LD0/MMXU1.MinWPhs", FunctionalConstraint::MX},
                {"LD0/MMXU1.MaxWPhs", FunctionalConstraint::MX},
                {"LD0/MMXU1.AvWPhs", FunctionalConstraint::MX}
            }
        },
        DataSetModel{
            .name = "DataSetSetpoints",
            .reference = "LD0/LLN0.DataSetSetpoints",
            .members = {
                {"LD0/DWMX1.SptReas", FunctionalConstraint::ST},
                {"LD0/DWMX1.WMaxFto.setVal", FunctionalConstraint::SP},
                {"LD0/DWMX1.WMaxSet.setMag.f", FunctionalConstraint::SP},
                {"LD0/DWMX1.WMaxSetPct.setMag.f", FunctionalConstraint::SP},
                {"LD0/DWMX1.WMaxSpt", FunctionalConstraint::MX},
                {"LD0/DWMX1.WMaxSptPct", FunctionalConstraint::MX},
                {"LD0/DGEN1.DEROpSt", FunctionalConstraint::ST}
            }
        },
        DataSetModel{
            .name = "DataSetActualValues",
            .reference = "LD0/LLN0.DataSetActualValues",
            .members = {
                {"LD0/MMXU1.TotW", FunctionalConstraint::MX},
                {"LD0/MMXU1.TotVAr", FunctionalConstraint::MX},
                {"LD0/MMXU1.PhV.phsA", FunctionalConstraint::MX},
                {"LD0/MMXU1.PhV.phsB", FunctionalConstraint::MX},
                {"LD0/MMXU1.PhV.phsC", FunctionalConstraint::MX},
                {"LD0/MMXU1.PPV.phsAB", FunctionalConstraint::MX},
                {"LD0/MMXU1.PPV.phsBC", FunctionalConstraint::MX},
                {"LD0/MMXU1.PPV.phsCA", FunctionalConstraint::MX},
                {"LD0/MMXU1.A.phsA", FunctionalConstraint::MX},
                {"LD0/MMXU1.A.phsB", FunctionalConstraint::MX},
                {"LD0/MMXU1.A.phsC", FunctionalConstraint::MX}
            }
        }
    };

    lln0.report_controls = {
        ReportControlState{
            .reference = ObjectReference{"LD0/LLN0.rcbMinMaxAvg"},
            .report_id = "MinMaxAvg",
            .enabled = false,
            .buffered = true,
            .data_set = "LD0/LLN0.DataSetMinMaxAvg",
            .conf_rev = 1,
            .buffer_time_ms = 0,
            .integrity_period_ms = 2000,
            .triggers = TriggerOptions{
                .integrity = true
            },
            .optional_fields = ReportOptionalFields{
                .sequence_number = false,
                .timestamp = true,
                .data_set = true,
                .buffer_overflow = true,
                .config_revision = false,
                .entry_id = true,
                .data_reference = false,
                .reason_code = false
            },
            .entry_id = Bytes(8, 0),
            .time_of_entry = deterministic_timestamp()
        },
        ReportControlState{
            .reference = ObjectReference{"LD0/LLN0.rcbSetpoints"},
            .report_id = "Setpoints",
            .enabled = false,
            .buffered = false,
            .data_set = "LD0/LLN0.DataSetSetpoints",
            .conf_rev = 1,
            .buffer_time_ms = 100,
            .integrity_period_ms = 1000,
            .triggers = TriggerOptions{
                .data_change = true,
                .integrity = true,
                .general_interrogation = true
            },
            .optional_fields = ReportOptionalFields{
                .timestamp = true,
                .reason_code = true
            }
        },
        ReportControlState{
            .reference = ObjectReference{"LD0/LLN0.rcbActualValues"},
            .report_id = "DataSetActualValues",
            .enabled = false,
            .buffered = false,
            .data_set = "LD0/LLN0.DataSetActualValues",
            .conf_rev = 1,
            .buffer_time_ms = 1000,
            .integrity_period_ms = 1000,
            .triggers = TriggerOptions{
                .data_change = true,
                .quality_change = true,
                .general_interrogation = true
            },
            .optional_fields = ReportOptionalFields{
                .timestamp = true,
                .config_revision = true,
                .reason_code = true
            }
        }
    };

    ld0.logical_nodes = {
        std::move(lln0), std::move(lphd1), std::move(dwmx1),
        std::move(dgen1), std::move(mmxu1)
    };

    model.logical_devices_.push_back(std::move(ld0));

    // Deterministic, visible values make lab behavior obvious.
    model.set_float("LD0/MMXU1.TotW.mag.f", 10.0F);
    model.set_float("LD0/MMXU1.TotVAr.mag.f", 20.0F);
    model.set_float("LD0/DWMX1.WMaxSpt.mxVal.f", 6.0F);

    return model;
}

} // namespace ar61850::dms
