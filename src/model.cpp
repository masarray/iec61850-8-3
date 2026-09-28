#include "ar61850/dms/model.hpp"

#include <algorithm>

namespace ar61850::dms {
namespace {

std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : value) {
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

DataObjectNode simple_do(std::string name) {
    return DataObjectNode{std::move(name), {}, {}};
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
    const DataObjectNode* current = nullptr;
    auto first = std::find_if(ln->data_objects.begin(), ln->data_objects.end(),
        [&](const auto& item) { return item.name == path[0]; });
    if (first == ln->data_objects.end()) return nullptr;
    current = &*first;
    for (std::size_t i = 1; i < path.size(); ++i) {
        const auto next = std::find_if(current->children.begin(), current->children.end(),
            [&](const auto& item) { return item.name == path[i]; });
        if (next == current->children.end()) return nullptr;
        current = &*next;
    }
    return current;
}

IedModel IedModel::make_ft20_reference_model() {
    IedModel model{"IED1"};

    LogicalDeviceModel ld0;
    ld0.name = "LD0";

    LogicalNodeModel lln0{"LLN0", {
        simple_do("Mod"), simple_do("NamPlt"), simple_do("Beh"), simple_do("Health")
    }};
    LogicalNodeModel lphd1{"LPHD1", {simple_do("PhyHealth"), simple_do("Proxy")}};
    LogicalNodeModel dwmx1{"DWMX1", {simple_do("Beh"), simple_do("WMaxSpt")}};
    LogicalNodeModel dgen1{"DGEN1", {simple_do("Beh")}};

    DataObjectNode totw{"TotW", {}, {
        {"mag", FunctionalConstraint::MX}, {"q", FunctionalConstraint::MX}, {"t", FunctionalConstraint::MX}
    }};
    DataObjectNode totvar{"TotVAr", {}, {
        {"mag", FunctionalConstraint::MX}, {"q", FunctionalConstraint::MX}, {"t", FunctionalConstraint::MX}
    }};
    LogicalNodeModel mmxu1{"MMXU1", {
        simple_do("Beh"), std::move(totw), std::move(totvar), simple_do("PhV"), simple_do("PPV"),
        simple_do("A"), simple_do("AvWPhs"), simple_do("MaxWPhs"), simple_do("MinWPhs")
    }};

    ld0.logical_nodes = {std::move(lln0), std::move(lphd1), std::move(dwmx1), std::move(dgen1), std::move(mmxu1)};
    model.logical_devices_.push_back(std::move(ld0));
    return model;
}

} // namespace ar61850::dms
