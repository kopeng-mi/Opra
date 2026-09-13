#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "sim/component.h"

namespace opra {

/** Every design assets/designs.json names, by name. A malformed file is fatal on boot (§7.3). */
class DesignStore {
public:
    void load(const std::string &path);
    const ShipDesign &design(const std::string &name) const;
    const std::vector<std::string> &names() const { return names_; }
    bool has(const std::string &name) const;

private:
    std::vector<std::string> names_;
    std::unordered_map<std::string, ShipDesign> designs_;
};

}  // namespace opra
