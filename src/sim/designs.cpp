#include "sim/designs.h"

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace opra {

namespace {

Facing parse_facing(const std::string &str, const std::string &file, size_t index) {
    if (str == "fore") return Facing::Fore;
    if (str == "aft") return Facing::Aft;
    if (str == "starboard") return Facing::Starboard;
    if (str == "port") return Facing::Port;
    if (str == "dorsal") return Facing::Dorsal;
    if (str == "ventral") return Facing::Ventral;
    throw std::runtime_error(file + ": placement " + std::to_string(index) +
                             " has unknown facing '" + str + "'");
}

}  // namespace

void DesignStore::load(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("DesignStore: failed to open " + path);
    }
    nlohmann::json root;
    try {
        file >> root;
    } catch (const std::exception &e) {
        throw std::runtime_error(path + ": json parse error: " + e.what());
    }

    if (!root.contains("designs") || !root["designs"].is_array()) {
        throw std::runtime_error(path + ": missing 'designs' array");
    }

    names_.clear();
    designs_.clear();

    for (const auto &d_json : root["designs"]) {
        ShipDesign d;
        d.name = d_json.value("name", "");
        d.role = d_json.value("role", "");
        d.shipClass = d_json.value("class", "");
        d.slots = d_json.value("slots", 12);
        d.pitch = d_json.value("pitch", 4.0);
        d.half_width = d_json.value("half_width", 1.6);
        d.recess = d_json.value("recess", 0.0);
        d.scale = d_json.value("scale", 1.0);
        d.cargo = d_json.value("cargo", 0.0);
        d.scan_scale = d_json.value("scan_scale", 1.0);
        d.collect_scale = d_json.value("collect_scale", 1.0);

        if (d.name.empty()) {
            throw std::runtime_error(path + ": design missing 'name'");
        }
        if (d.slots > 16 || d.slots < 1) {
            throw std::runtime_error(path + ": design '" + d.name + "' declares " +
                                     std::to_string(d.slots) + " slots (max 16)");
        }

        d.spine.name = d.name;
        d.spine.slots = d.slots;
        d.spine.pitch = d.pitch;
        d.spine.half_width = d.half_width;
        d.spine.recess = d.recess;

        if (d_json.contains("placements") && d_json["placements"].is_array()) {
            size_t idx = 0;
            for (const auto &p_json : d_json["placements"]) {
                Placement p;
                p.part = p_json.value("part", "");
                p.slot = p_json.value("slot", 0);
                const std::string facing_str = p_json.value("facing", "");
                p.facing = parse_facing(facing_str, path, idx);
                p.roll = p_json.value("roll", 0);
                p.span = p_json.value("span", 1);
                p.group = p_json.value("group", 0);
                p.axial = is_axial(p.facing);
                d.placements.push_back(p);
                ++idx;
            }
        }

        names_.push_back(d.name);
        designs_[d.name] = d;
    }
}

const ShipDesign &DesignStore::design(const std::string &name) const {
    auto it = designs_.find(name);
    if (it != designs_.end()) return it->second;
    for (const auto &[k, v] : designs_) {
        if (v.shipClass == name) return v;
        if (k.size() == name.size()) {
            bool match = true;
            for (size_t i = 0; i < k.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(k[i])) !=
                    std::tolower(static_cast<unsigned char>(name[i]))) {
                    match = false;
                    break;
                }
            }
            if (match) return v;
        }
    }
    throw std::runtime_error("DesignStore: unknown design '" + name + "'");
}

bool DesignStore::has(const std::string &name) const {
    if (designs_.find(name) != designs_.end()) return true;
    for (const auto &[k, v] : designs_) {
        if (v.shipClass == name) return true;
    }
    return false;
}

}  // namespace opra
