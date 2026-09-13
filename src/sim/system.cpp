#include "sim/system.h"

#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

#include "core/log.h"
#include "orbit/lagrange.h"
#include "orbit/soi.h"

namespace opra {
namespace {

using json = nlohmann::json;

[[noreturn]] void bad_field(const std::string &path, const std::string &field,
                            const std::string &why) {
    const std::string message = "system " + path + ": field '" + field + "' " + why;
    SDL_Log("%s", message.c_str());
    std::fprintf(stderr, "%s\n", message.c_str());
    std::exit(1);
}

double number(const json &at, const char *field, const std::string &path, double fallback,
              bool required) {
    if (!at.contains(field)) {
        if (required) bad_field(path, field, "is missing");
        return fallback;
    }
    if (!at[field].is_number()) bad_field(path, field, "is not a number");
    return at[field].get<double>();
}

/**
 * The world fields (plan 03): atmosphere for drag and heating, terrain for the surface, a Lagrange
 * seat for a station that is not on a conic, and a surface seat for a base on a pad. All four are
 * optional and all four are validated when present: a hand-edited system file that names a pad
 * index that does not exist is a fatal error, not a base that silently floats.
 */
void parse_world_fields(const json &entry, Body &body, const SystemDef &system,
                        const std::string &path, const char *section) {
    const auto field = [&](const char *name) { return std::string(section) + "." + name; };

    if (entry.contains("atmosphere")) {
        const json &air = entry["atmosphere"];
        body.atmosphere.present = true;
        body.atmosphere.rho0 = number(air, "rho0", path, 0.0, true);
        body.atmosphere.scale_height = number(air, "H", path, 0.0, true);
        body.atmosphere.top = number(air, "top", path, 0.0, true);
        if (body.atmosphere.rho0 <= 0.0 || body.atmosphere.scale_height <= 0.0 ||
            body.atmosphere.top <= 0.0) {
            bad_field(path, field("atmosphere"),
                      "needs positive rho0, H and top: an atmosphere with no top never ends");
        }
    }

    if (entry.contains("terrain")) {
        const json &land = entry["terrain"];
        body.terrain.present = true;
        body.terrain.seed = static_cast<unsigned int>(number(land, "seed", path, 0.0, true));
        body.terrain.amplitude = number(land, "amplitude", path, 0.004, false);
        if (land.contains("pads")) {
            for (const json &pad : land["pads"]) {
                Pad out;
                out.theta = number(pad, "theta", path, 0.0, true);
                out.width = number(pad, "width", path, 0.0, true);
                out.name = pad.value("name", std::string{"pad"});
                if (out.width <= 0.0) bad_field(path, field("terrain.pads[].width"), "is not positive");
                body.terrain.pads.push_back(out);
            }
        }
    }

    if (entry.contains("lagrange")) {
        const json &point = entry["lagrange"];
        body.lagrange.present = true;
        const std::string primary = point.value("primary", std::string{});
        body.lagrange.primary = system.index_of(primary);
        if (body.lagrange.primary < 0) {
            bad_field(path, field("lagrange.primary"), "does not name an earlier body");
        }
        body.lagrange.point = static_cast<int>(number(point, "point", path, 4.0, true));
        if (body.lagrange.point < 1 || body.lagrange.point > 5) {
            bad_field(path, field("lagrange.point"), "is not one of L1..L5");
        }
        body.lagrange.dtheta = number(point, "dtheta", path, 0.0, false);
    }

    if (entry.contains("maps")) {
        // The body's own maps (plan-04 s3.4): names in assets/textures/manifest.json. A body with
        // none keeps its procedural shading.
        const json &maps = entry["maps"];
        body.albedo_map = maps.value("albedo", std::string{});
        body.cloud_map = maps.value("clouds", std::string{});
        body.night_map = maps.value("night", std::string{});
        body.photosphere_map = maps.value("photosphere", std::string{});
    }

    body.tint = entry.value("color", std::string{});

    if (entry.contains("surface")) {
        const json &seat = entry["surface"];
        body.surface.present = true;
        body.surface.pad = static_cast<int>(number(seat, "pad", path, 0.0, true));
        if (body.surface.pad < 0 ||
            static_cast<size_t>(body.surface.pad) >= body.terrain.pads.size()) {
            bad_field(path, field("surface.pad"), "does not name one of this body's pads");
        }
        body.surface.name = seat.value("name", body.terrain.pads[static_cast<size_t>(body.surface.pad)].name);
        body.surface.model = seat.value("model", std::string{});
    }
}

}  // namespace

int SystemDef::index_of(const std::string &id) const {
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

SystemDef load_system(const std::string &path) {
    std::ifstream file(path);
    if (!file) bad_field(path, "(file)", "cannot be opened");
    json root;
    try {
        file >> root;
    } catch (const std::exception &error) {
        bad_field(path, "(json)", std::string("does not parse: ") + error.what());
    }

    SystemDef system;
    system.name = root.value("name", std::string{"unnamed"});
    system.epoch = root.value("epoch", 0.0);
    if (root.contains("jump_links")) system.jump_links = root["jump_links"].get<std::vector<std::string>>();

    // The star is body 0: the frame is anchored on it, and it has no parent to orbit.
    if (!root.contains("star")) bad_field(path, "star", "is missing");
    const json &star = root["star"];
    Body anchor;
    anchor.id = "nereid";
    anchor.name = star.value("name", std::string{"star"});
    anchor.kind = BodyClass::Star;
    anchor.parent = -1;
    anchor.mu = number(star, "mu", path, 0.0, true);
    anchor.radius = number(star, "radius", path, 0.0, true);
    anchor.soi = 0.0;  // the root: everything is inside it by definition
    if (star.contains("maps")) {
        anchor.photosphere_map = star["maps"].value("photosphere", std::string{});
    }
    anchor.tint = star.value("color", std::string{});
    system.bodies.push_back(anchor);

    if (!root.contains("bodies") || !root["bodies"].is_array()) {
        bad_field(path, "bodies", "is missing or not an array");
    }
    for (const json &entry : root["bodies"]) {
        Body body;
        body.id = entry.value("id", std::string{});
        if (body.id.empty()) bad_field(path, "bodies[].id", "is missing");
        body.name = entry.value("name", body.id);
        const std::string kind = entry.value("kind", std::string{"planet"});
        body.kind = kind == "moon"    ? BodyClass::Moon
                    : kind == "star"  ? BodyClass::Star
                    : kind == "station" ? BodyClass::Station
                                        : BodyClass::Planet;
        body.mu = number(entry, "mu", path, 0.0, true);
        body.radius = number(entry, "radius", path, 0.0, true);
        body.model = entry.value("model", std::string{});
        if (body.model.empty()) bad_field(path, "bodies[].model", "is missing");

        const std::string parent_id = entry.value("parent", std::string{"nereid"});
        body.parent = system.index_of(parent_id);
        if (body.parent < 0) bad_field(path, "bodies[].parent", "does not name an earlier body");

        const Body &parent = system.bodies[static_cast<size_t>(body.parent)];
        body.elements.a = number(entry, "a", path, 0.0, true);
        body.elements.e = number(entry, "e", path, 0.0, true);
        body.elements.omega = number(entry, "omega", path, 0.0, true);
        body.elements.M0 = number(entry, "M0", path, 0.0, true);
        body.elements.t0 = system.epoch;
        // The elements are about the parent, so the mu the propagation uses is the parent's.
        body.elements.mu = parent.mu;

        if (body.elements.a <= parent.radius) {
            bad_field(path, "bodies[].a", "is inside its parent's radius");
        }
        if (!(body.elements.e >= 0.0 && body.elements.e < 1.0)) {
            bad_field(path, "bodies[].e", "is outside [0, 1): the system is elliptic by definition");
        }
        if (parent.mu > 0.0 && body.mu > 0.0) {
            body.soi = orbit::soi_radius(body.elements.a, body.mu, parent.mu);
        }
        parse_world_fields(entry, body, system, path, "bodies[]");
        system.bodies.push_back(body);
    }

    // Stations are bodies too: they orbit, they are drawn, and they hold nothing in orbit.
    if (root.contains("stations")) {
        for (const json &entry : root["stations"]) {
            Body body;
            body.id = entry.value("id", std::string{});
            if (body.id.empty()) bad_field(path, "stations[].id", "is missing");
            body.name = entry.value("name", body.id);
            body.kind = BodyClass::Station;
            body.mu = 0.0;  // no gravity of its own: a target, not a primary
            body.radius = number(entry, "radius", path, 60.0, false);
            body.model = entry.value("model", std::string{});
            if (body.model.empty()) bad_field(path, "stations[].model", "is missing");
            const std::string parent_id = entry.value("parent", std::string{"nereid"});
            body.parent = system.index_of(parent_id);
            if (body.parent < 0) bad_field(path, "stations[].parent", "does not name an earlier body");
            const Body &parent = system.bodies[static_cast<size_t>(body.parent)];
            body.elements.a = number(entry, "a", path, 0.0, true);
            body.elements.e = number(entry, "e", path, 0.0, true);
            body.elements.omega = number(entry, "omega", path, 0.0, true);
            body.elements.M0 = number(entry, "M0", path, 0.0, true);
            body.elements.t0 = system.epoch;
            body.elements.mu = parent.mu;
            if (body.elements.a <= parent.radius) {
                bad_field(path, "stations[].a", "is inside its parent's radius");
            }
            parse_world_fields(entry, body, system, path, "stations[]");
            system.bodies.push_back(body);
        }
    }

    if (!root.contains("belt")) bad_field(path, "belt", "is missing");
    const json &belt = root["belt"];
    system.belt.id = belt.value("id", std::string{"belt"});
    const std::string belt_parent = belt.value("parent", std::string{"nereid"});
    system.belt.parent = system.index_of(belt_parent);
    if (system.belt.parent < 0) bad_field(path, "belt.parent", "does not name a body");
    system.belt.inner = number(belt, "inner", path, 0.0, true);
    system.belt.outer = number(belt, "outer", path, 0.0, true);
    system.belt.count = static_cast<int>(number(belt, "count", path, 0.0, true));
    system.belt.seed = static_cast<unsigned int>(number(belt, "seed", path, 0.0, true));
    const Body &belt_primary = system.bodies[static_cast<size_t>(system.belt.parent)];
    if (system.belt.inner <= belt_primary.radius || system.belt.outer <= system.belt.inner) {
        bad_field(path, "belt", "has an inner radius below its primary or an outer below its inner");
    }
    return system;
}

void propagate(const SystemDef &system, double t, std::vector<BodyState> &out) {
    out.resize(system.bodies.size());
    for (size_t i = 0; i < system.bodies.size(); ++i) {
        const Body &body = system.bodies[i];
        if (body.parent < 0) {
            // The star is the frame's anchor: it has no elements and no parent to orbit, so its
            // state is the origin. Asking a conic solver about a root body would divide by zero.
            out[i].position = glm::dvec2(0.0);
            out[i].velocity = glm::dvec2(0.0);
            continue;
        }
        // A Lagrange seat is a co-rotating placement, not a conic (plan 3.5): its own elements
        // would run at the wrong period and drift off the point within days. It holds the
        // secondary's angular position plus its own offset, at the point's fixed radius from the
        // primary, and its velocity is the rigid rotation of that radius.
        if (body.lagrange.present && body.lagrange.primary >= 0) {
            const Body &secondary = system.bodies[static_cast<size_t>(body.lagrange.primary)];
            if (secondary.parent >= 0) {
                const BodyState &anchor = out[static_cast<size_t>(secondary.parent)];
                const BodyState &at = out[static_cast<size_t>(body.lagrange.primary)];
                const glm::dvec2 arm = at.position - anchor.position;
                const double mu = orbit::mass_ratio(system.bodies[static_cast<size_t>(secondary.parent)].mu,
                                                    secondary.mu);
                const glm::dvec2 place = orbit::lagrange_offset(
                    mu, secondary.elements.a, body.lagrange.point, body.lagrange.dtheta);
                const double angle = std::atan2(arm.y, arm.x);
                const glm::dvec2 turned(place.x * std::cos(angle) - place.y * std::sin(angle),
                                        place.x * std::sin(angle) + place.y * std::cos(angle));
                const glm::dvec2 arm_velocity = at.velocity - anchor.velocity;
                const double rate = (arm.x * arm_velocity.y - arm.y * arm_velocity.x) /
                                    glm::dot(arm, arm);
                out[i].position = anchor.position + turned;
                out[i].velocity = anchor.velocity + glm::dvec2(-turned.y, turned.x) * rate;
                continue;
            }
        }
        const orbit::State relative = orbit::state_at(body.elements, t);
        const BodyState &parent = out[static_cast<size_t>(body.parent)];
        out[i].position = parent.position + relative.r;
        out[i].velocity = parent.velocity + relative.v;
    }
}

int primary_of(const SystemDef &system, const std::vector<BodyState> &states, int current,
               const glm::dvec2 &position) {
    // Walk down from the star: each step looks only at the children of the body we are currently
    // in, so a ship in a moon's SOI is not tested against the moon's siblings. `here` is a body
    // index; the return is -1 for the star, which is what the caller stores.
    int here = 0;
    for (int step = 0; step < static_cast<int>(system.bodies.size()); ++step) {
        int next = -1;
        for (size_t i = 1; i < system.bodies.size(); ++i) {
            const Body &body = system.bodies[i];
            if (body.parent != here || body.soi <= 0.0) continue;
            const double distance = glm::length(position - states[i].position);
            // Hysteresis: the body we are already in keeps the ship until it is clearly outside,
            // so a boundary skim does not switch frames every frame.
            const double limit = (static_cast<int>(i) == current) ? body.soi * 1.02 : body.soi;
            if (distance < limit) {
                next = static_cast<int>(i);
                break;
            }
        }
        if (next < 0) break;
        here = next;
    }
    return here == 0 ? -1 : here;
}

}  // namespace opra
