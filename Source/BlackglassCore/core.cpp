#include "blackglass/core.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace blackglass {
namespace {
constexpr double tick_seconds = 0.02;
constexpr double epsilon = 1e-8;
bool finite(Vec2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
Vec2 toward(Vec2 a, Vec2 b, double step) {
    const double length = distance(a, b);
    if (length <= step || length < epsilon) return b;
    return {a.x + (b.x - a.x) * step / length, a.y + (b.y - a.y) * step / length};
}
const std::array<Cell, 8> neighbors{{{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{-1,-1},{1,-1}}};
bool opposed(Kind a, Kind b) {
    return (a == Kind::operative && b == Kind::security) || (a == Kind::security && b == Kind::operative);
}
bool valid_kind(int k) { return k >= 0 && k <= static_cast<int>(Kind::specialist); }
bool valid_order(int k) { return k >= 0 && k <= static_cast<int>(OrderKind::extract); }
Result replace_file(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return Result::failure("Atomic replacement failed: Windows error " + std::to_string(GetLastError()));
#else
    std::error_code error;
    std::filesystem::rename(from, to, error);
    if (error) return Result::failure("Atomic replacement failed: " + error.message());
#endif
    return Result::success();
}
} // namespace
double distance(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
Result Result::success() { return {true, {}}; }
Result Result::failure(std::string reason) { return {false, std::move(reason)}; }

Grid::Grid(int width, int height) : width_(width), height_(height) {
    if (width < 1 || height < 1 || width > 256 || height > 256) throw std::invalid_argument("Grid dimensions must be 1..256.");
    blocked_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
}
bool Grid::inside(Cell c) const { return c.x >= 0 && c.y >= 0 && c.x < width_ && c.y < height_; }
bool Grid::walkable(Cell c) const { return inside(c) && !blocked_[static_cast<std::size_t>(c.y * width_ + c.x)]; }
void Grid::set_blocked(Cell c, bool blocked) {
    if (!inside(c)) throw std::out_of_range("Cell outside navigation fixture.");
    blocked_[static_cast<std::size_t>(c.y * width_ + c.x)] = blocked ? 1 : 0;
}
Cell Grid::cell_at(Vec2 p) {
    if (!finite(p) || std::abs(p.x) > 1000000 || std::abs(p.y) > 1000000)
        throw std::invalid_argument("Position is not finite or is outside supported bounds.");
    return {static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y))};
}
Vec2 Grid::center(Cell c) { return {c.x + 0.5, c.y + 0.5}; }
std::vector<Cell> Grid::path(Cell start, Cell goal) const {
    if (!walkable(start) || !walkable(goal)) return {};
    const int total = width_ * height_;
    const auto index = [this](Cell c) { return c.y * width_ + c.x; };
    std::vector<int> parent(static_cast<std::size_t>(total), -1);
    std::queue<Cell> open;
    open.push(start);
    parent[static_cast<std::size_t>(index(start))] = index(start);
    while (!open.empty() && parent[static_cast<std::size_t>(index(goal))] == -1) {
        const Cell current = open.front(); open.pop();
        // Cardinal steps deliberately avoid corner clipping.
        for (std::size_t i = 0; i < 4; ++i) {
            const Cell next{current.x + neighbors[i].x, current.y + neighbors[i].y};
            if (!walkable(next) || parent[static_cast<std::size_t>(index(next))] != -1) continue;
            parent[static_cast<std::size_t>(index(next))] = index(current);
            open.push(next);
        }
    }
    int at = index(goal);
    if (parent[static_cast<std::size_t>(at)] == -1) return {};
    std::vector<Cell> result;
    while (at != index(start)) {
        result.push_back({at % width_, at / width_});
        at = parent[static_cast<std::size_t>(at)];
    }
    result.push_back(start);
    std::reverse(result.begin(), result.end());
    return result;
}
bool Grid::clear_line(Vec2 from, Vec2 to) const {
    if (!finite(from) || !finite(to)) return false;
    Cell at = cell_at(from);
    const Cell end = cell_at(to);
    if (!walkable(at) || !walkable(end)) return false;
    const double dx = to.x - from.x, dy = to.y - from.y;
    const int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0), sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    const double inf = std::numeric_limits<double>::infinity();
    const double txDelta = sx ? 1.0 / std::abs(dx) : inf, tyDelta = sy ? 1.0 / std::abs(dy) : inf;
    double tx = sx ? ((sx > 0 ? at.x + 1.0 : at.x) - from.x) / dx : inf;
    double ty = sy ? ((sy > 0 ? at.y + 1.0 : at.y) - from.y) / dy : inf;
    for (int n = 0; n <= width_ + height_ + 2 && at != end; ++n) {
        if (std::abs(tx - ty) < epsilon) {
            // Supercover: touching an occupied corner blocks a shot.
            if (!walkable({at.x + sx, at.y}) || !walkable({at.x, at.y + sy})) return false;
            at.x += sx; at.y += sy; tx += txDelta; ty += tyDelta;
        } else if (tx < ty) { at.x += sx; tx += txDelta; }
        else { at.y += sy; ty += tyDelta; }
        if (!walkable(at)) return false;
    }
    return at == end;
}

Result read_weapons(std::istream& input, std::map<std::string, WeaponDefinition>& output) {
    std::map<std::string, WeaponDefinition> candidate;
    std::string line;
    if (!std::getline(input, line)) return Result::failure("Missing weapon CSV header.");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "id,range,damage,interval,sound_radius,weight") return Result::failure("Unsupported weapon CSV columns.");
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.size() > 512 || std::count(line.begin(), line.end(), ',') != 5) return Result::failure("Weapon CSV needs exactly six columns.");
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        WeaponDefinition w;
        if (!(row >> w.id >> w.range >> w.damage >> w.interval_seconds >> w.sound_radius >> w.weight))
            return Result::failure("Invalid weapon CSV row.");
        row >> std::ws;
        if (!row.eof() || w.id.size() > 64 || !std::isfinite(w.range) || !std::isfinite(w.damage) ||
            !std::isfinite(w.interval_seconds) || !std::isfinite(w.sound_radius) || !std::isfinite(w.weight) ||
            w.range <= 0 || w.range > 1000 || w.damage <= 0 || w.damage > 10000 ||
            w.interval_seconds < tick_seconds || w.interval_seconds > 60 || w.sound_radius < 0 || w.sound_radius > 1000 ||
            w.weight <= 0 || w.weight > 1000 || candidate.count(w.id))
            return Result::failure("Weapon values are invalid or duplicated.");
        candidate.emplace(w.id, w);
        if (candidate.size() > 256) return Result::failure("Too many weapon definitions.");
    }
    if (candidate.empty()) return Result::failure("No weapon definitions.");
    output = std::move(candidate);
    return Result::success();
}

World::World(Grid grid) : grid_(std::move(grid)) {}
EntityId World::add_actor(Kind kind, Vec2 position) {
    if (!grid_.walkable(Grid::cell_at(position))) throw std::invalid_argument("Spawn point is blocked.");
    if (kind == Kind::operative && std::count_if(actors_.begin(), actors_.end(), [](const auto& p) { return p.second.kind == Kind::operative; }) >= 4)
        throw std::invalid_argument("Mission squad is limited to four operatives.");
    const EntityId id = next_id_++;
    Actor value; value.id = id; value.kind = kind; value.position = position;
    actors_.emplace(id, std::move(value));
    return id;
}
EntityId World::add_vehicle(Vec2 position) {
    if (!grid_.walkable(Grid::cell_at(position))) throw std::invalid_argument("Vehicle spawn point is blocked.");
    const EntityId id = next_id_++;
    Vehicle value; value.id = id; value.position = position;
    vehicles_.emplace(id, std::move(value));
    return id;
}
Actor* World::actor(EntityId id) { const auto i = actors_.find(id); return i == actors_.end() ? nullptr : &i->second; }
const Actor* World::actor(EntityId id) const { const auto i = actors_.find(id); return i == actors_.end() ? nullptr : &i->second; }
Vehicle* World::vehicle(EntityId id) { const auto i = vehicles_.find(id); return i == vehicles_.end() ? nullptr : &i->second; }
const Vehicle* World::vehicle(EntityId id) const { const auto i = vehicles_.find(id); return i == vehicles_.end() ? nullptr : &i->second; }
void World::push_event(std::string type, EntityId source, EntityId target, std::string detail) {
    if (events_.size() >= 256) events_.erase(events_.begin(), events_.begin() + 64);
    events_.push_back({time_, std::move(type), source, target, std::move(detail)});
}
Result World::select(const std::vector<EntityId>& ids, bool additive) {
    for (const auto id : ids) {
        const auto* a = actor(id);
        if (!a || a->kind != Kind::operative || !a->alive()) return Result::failure("Only living squad operatives can be selected.");
    }
    if (!additive) for (auto& pair : actors_) pair.second.selected = false;
    for (const auto id : ids) actor(id)->selected = true;
    return Result::success();
}
Result World::issue(const std::vector<EntityId>& ids, Order order, bool queued) {
    if (ids.empty()) return Result::failure("No operatives selected.");
    if (!valid_order(static_cast<int>(order.kind)) || !finite(order.destination)) return Result::failure("Invalid order.");
    std::set<EntityId> unique;
    std::vector<std::pair<EntityId, Order>> pending;
    std::set<std::pair<int, int>> reserved;
    for (const auto id : ids) {
        auto* a = actor(id);
        if (!unique.insert(id).second || !a || a->kind != Kind::operative || !a->alive())
            return Result::failure("Invalid, duplicate or dead operative.");
        if (queued && a->orders.size() >= 256) return Result::failure("Order queue is full.");
        Order next = order;
        if (order.kind == OrderKind::drive) next.destination = Grid::center(Grid::cell_at(order.destination));
        if (order.kind == OrderKind::move) {
            if (a->vehicle) return Result::failure("Use a vehicle drive order while boarded.");
            const Cell goal = Grid::cell_at(order.destination);
            bool found = false;
            for (int radius = 0; radius < 4 && !found; ++radius) {
                for (int y = -radius; y <= radius && !found; ++y) {
                    for (int x = -radius; x <= radius && !found; ++x) {
                        if (std::abs(x) + std::abs(y) != radius) continue;
                        const Cell c{goal.x + x, goal.y + y};
                        if (reserved.count({c.x, c.y}) || grid_.path(Grid::cell_at(a->position), c).empty()) continue;
                        next.destination = Grid::center(c);
                        reserved.insert({c.x, c.y}); found = true;
                    }
                }
            }
            if (!found) return Result::failure("No reachable formation position.");
        }
        if (order.kind == OrderKind::attack) {
            const auto* target = actor(order.target);
            if (!target || !target->alive() || order.target == id) return Result::failure("Attack target is invalid or dead.");
        }
        if (order.kind == OrderKind::board && (!vehicle(order.target) || vehicle(order.target)->destroyed()))
            return Result::failure("Vehicle is missing or destroyed.");
        if (order.kind == OrderKind::acquire && (!actor(order.target) || actor(order.target)->kind != Kind::specialist || !actor(order.target)->alive()))
            return Result::failure("Specialist is missing or dead.");
        pending.emplace_back(id, next);
    }
    for (const auto& value : pending) {
        auto& a = *actor(value.first);
        if (!queued || order.kind == OrderKind::stop) { a.orders.clear(); a.route.clear(); }
        if (order.kind == OrderKind::stop && a.vehicle && vehicle(a.vehicle)->occupants[0] == a.id) vehicle(a.vehicle)->route.clear();
        if (order.kind != OrderKind::stop) a.orders.push_back(value.second);
    }
    return Result::success();
}
double World::carried_weight(const Actor& a) const {
    double weight = 0;
    for (const auto& slot : a.inventory) {
        const auto i = weapons_.find(slot.definition);
        if (i != weapons_.end()) weight += i->second.weight;
    }
    return weight;
}
Result World::equip(EntityId id, std::size_t slot, const std::string& weapon_id, int ammunition) {
    auto* a = actor(id);
    const auto w = weapons_.find(weapon_id);
    if (!a || !a->alive() || slot >= a->inventory.size() || w == weapons_.end() || ammunition < 0 || ammunition > 100000)
        return Result::failure("Invalid operative, slot, weapon or ammunition.");
    const auto old = weapons_.find(a->inventory[slot].definition);
    const double removed = old == weapons_.end() ? 0.0 : old->second.weight;
    if (carried_weight(*a) - removed + w->second.weight > a->capacity) return Result::failure("Carrying capacity exceeded.");
    a->inventory[slot] = {weapon_id, ammunition};
    return Result::success();
}
Result World::switch_weapon(EntityId id, std::size_t slot) {
    auto* a = actor(id);
    if (!a || !a->alive() || slot >= a->inventory.size() || a->inventory[slot].definition.empty())
        return Result::failure("No equipment in this slot.");
    a->active_slot = slot;
    return Result::success();
}
Result World::holster(EntityId id, bool holstered) {
    auto* a = actor(id);
    if (!a || !a->alive()) return Result::failure("Operative is missing or dead.");
    a->holstered = holstered;
    return Result::success();
}
Result World::transfer(EntityId from, std::size_t from_slot, EntityId to, std::size_t to_slot) {
    auto* source = actor(from); auto* destination = actor(to);
    if (!source || !destination || source == destination || !source->alive() || !destination->alive() ||
        from_slot >= source->inventory.size() || to_slot >= destination->inventory.size())
        return Result::failure("Invalid transfer participants or slots.");
    if (distance(source->position, destination->position) > 1.5 || !grid_.clear_line(source->position, destination->position))
        return Result::failure("Transfer requires adjacent operatives with a clear path.");
    if (source->inventory[from_slot].definition.empty() || !destination->inventory[to_slot].definition.empty())
        return Result::failure("Source must contain equipment and destination must be empty.");
    const auto w = weapons_.find(source->inventory[from_slot].definition);
    if (w == weapons_.end() || carried_weight(*destination) + w->second.weight > destination->capacity)
        return Result::failure("Recipient carrying capacity exceeded.");
    destination->inventory[to_slot] = std::move(source->inventory[from_slot]);
    source->inventory[from_slot] = {};
    return Result::success();
}
void World::notice_shot(EntityId source, Vec2 origin, double radius) {
    for (auto& pair : actors_) {
        auto& a = pair.second;
        if (a.id == source || !a.alive() || distance(a.position, origin) > radius) continue;
        a.last_known_threat = origin;
        if (a.kind == Kind::civilian || a.kind == Kind::specialist) a.awareness = Awareness::fleeing;
        if (a.kind == Kind::security) {
            if (actor(source) && actor(source)->kind == Kind::security) continue;
            if (grid_.clear_line(a.position, origin)) {
                a.awareness = Awareness::engaged;
                a.last_seen_enemy = source;
            } else a.awareness = Awareness::investigating;
        }
    }
    push_event("gunshot", source, no_entity, "Local audible event.");
}
Result World::shoot(EntityId attacker_id, EntityId target_id) {
    auto* a = actor(attacker_id); auto* target = actor(target_id);
    if (!a || !target || !a->alive() || !target->alive() || a == target) return Result::failure("Shooter or target is invalid.");
    if (a->vehicle || target->vehicle) return Result::failure("Actor firing from or into vehicle seats is not implemented.");
    const auto& slot = a->inventory[a->active_slot];
    const auto weapon = weapons_.find(slot.definition);
    if (weapon == weapons_.end()) return Result::failure("No weapon selected.");
    const auto& w = weapon->second;
    if (a->fire_cooldown > epsilon) return Result::failure("Weapon is recovering.");
    if (slot.ammunition <= 0) return Result::failure("Weapon is out of ammunition.");
    if (distance(a->position, target->position) > w.range) return Result::failure("Target is out of range.");
    if (!grid_.clear_line(a->position, target->position)) return Result::failure("Shot is blocked by geometry.");
    const Vec2 delta{target->position.x - a->position.x, target->position.y - a->position.y};
    const double length2 = delta.x * delta.x + delta.y * delta.y;
    double earliest = 1.0;
    Actor* hit = target;
    if (length2 > epsilon) for (auto& pair : actors_) {
        auto& other = pair.second;
        if (other.id == attacker_id || !other.alive() || other.vehicle) continue;
        const double t = ((other.position.x - a->position.x) * delta.x + (other.position.y - a->position.y) * delta.y) / length2;
        if (t < 0.0 || t > earliest) continue;
        const Vec2 closest{a->position.x + t * delta.x, a->position.y + t * delta.y};
        if (distance(other.position, closest) <= 0.3) { earliest = t; hit = &other; }
    }
    for (const auto& pair : vehicles_) {
        const auto& v = pair.second;
        const double t = length2 > epsilon ? ((v.position.x - a->position.x) * delta.x + (v.position.y - a->position.y) * delta.y) / length2 : 0;
        if (t >= 0 && t <= earliest && distance(v.position, {a->position.x + t * delta.x, a->position.y + t * delta.y}) < 0.65)
            return Result::failure("Shot is blocked by a vehicle; vehicle attack orders are not implemented.");
    }
    if (!friendly_fire && !opposed(a->kind, hit->kind)) return Result::failure("Friendly-fire protection blocks this shot.");
    --a->inventory[a->active_slot].ammunition;
    a->fire_cooldown = w.interval_seconds;
    a->holstered = false;
    hit->health = std::max(0.0, hit->health - std::max(1.0, w.damage - hit->armor));
    notice_shot(attacker_id, a->position, w.sound_radius);
    push_event("hit", attacker_id, hit->id, w.id);
    if (!hit->alive()) { hit->orders.clear(); hit->route.clear(); push_event("death", hit->id, no_entity); }
    check_failure();
    return Result::success();
}

Result World::board(EntityId actor_id, EntityId vehicle_id) {
    auto* a = actor(actor_id); auto* v = vehicle(vehicle_id);
    if (!a || !a->alive() || !v || v->destroyed()) return Result::failure("Actor or vehicle is unavailable.");
    if (a->vehicle) return Result::failure("Actor already occupies a vehicle.");
    if (distance(a->position, v->position) > 1.5 || !grid_.clear_line(a->position, v->position))
        return Result::failure("Boarding requires proximity and an unobstructed route.");
    const auto seat = std::find(v->occupants.begin(), v->occupants.end(), no_entity);
    if (seat == v->occupants.end()) return Result::failure("Vehicle is full.");
    *seat = actor_id; a->vehicle = vehicle_id; a->position = v->position;
    a->orders.clear(); a->route.clear();
    push_event("boarded", actor_id, vehicle_id);
    return Result::success();
}
Result World::disembark(EntityId id) {
    auto* a = actor(id);
    if (!a || !a->alive() || !a->vehicle) return Result::failure("No living occupant to disembark.");
    auto* v = vehicle(a->vehicle);
    if (!v) return Result::failure("Occupancy references a missing vehicle.");
    const Cell origin = Grid::cell_at(v->position);
    for (const Cell offset : neighbors) {
        const Cell exit{origin.x + offset.x, origin.y + offset.y};
        if (!grid_.walkable(exit) || !grid_.clear_line(v->position, Grid::center(exit))) continue;
        const Vec2 position = Grid::center(exit);
        bool occupied = false;
        for (const auto& pair : actors_)
            if (pair.second.id != id && pair.second.alive() && !pair.second.vehicle && distance(pair.second.position, position) < 0.6) occupied = true;
        for (const auto& pair : vehicles_)
            if (pair.first != v->id && distance(pair.second.position, position) < 0.9) occupied = true;
        if (occupied) continue;
        for (auto& seat : v->occupants) if (seat == id) seat = no_entity;
        const EntityId vehicle_id = a->vehicle;
        a->vehicle = no_entity; a->position = position; a->route.clear(); a->orders.clear();
        if (v->occupants[0] == no_entity) v->route.clear();
        push_event("disembarked", id, vehicle_id);
        return Result::success();
    }
    return Result::failure("All vehicle exits are obstructed. Occupancy is retained.");
}
Result World::drive(EntityId id, Vec2 destination) {
    auto* a = actor(id);
    auto* v = a ? vehicle(a->vehicle) : nullptr;
    if (!a || !a->alive() || !v || v->destroyed()) return Result::failure("No usable vehicle.");
    if (v->occupants[0] != id) return Result::failure("Only the driver in seat zero can move this vehicle.");
    const auto route = grid_.path(Grid::cell_at(v->position), Grid::cell_at(destination));
    if (route.empty()) return Result::failure("Vehicle destination is unreachable.");
    v->route.assign(route.begin() + 1, route.end());
    if (route.size() == 1 && distance(v->position, Grid::center(route.back())) > epsilon) v->route.push_back(route.back());
    return Result::success();
}
Result World::damage_vehicle(EntityId id, double amount) {
    auto* v = vehicle(id);
    if (!v || !std::isfinite(amount) || amount < 0.0) return Result::failure("Invalid vehicle damage.");
    if (v->destroyed()) return Result::success();
    v->health = std::max(0.0, v->health - amount);
    if (v->destroyed()) {
        v->route.clear();
        const auto occupants = v->occupants;
        for (const auto occupant : occupants) if (occupant != no_entity) {
            auto* a = actor(occupant);
            a->health = std::max(0.0, a->health - 80.0);
            if (a->alive()) {
                const auto result = disembark(occupant);
                if (!result.ok) push_event("trapped_in_wreck", occupant, id, result.reason);
            }
        }
        push_event("vehicle_destroyed", id, no_entity);
        check_failure();
    }
    return Result::success();
}
Result World::acquire(EntityId controller, EntityId specialist_id) {
    auto* a = actor(controller); auto* specialist = actor(specialist_id);
    if (mission_.state != MissionState::active) return Result::failure("Mission has already ended.");
    if (!a || a->kind != Kind::operative || !a->alive() || !specialist || !specialist->alive() ||
        specialist->kind != Kind::specialist || mission_.specialist != specialist_id)
        return Result::failure("Controller or mission specialist is unavailable.");
    if (distance(a->position, specialist->position) > 1.5 || !grid_.clear_line(a->position, specialist->position))
        return Result::failure("Acquire requires proximity and a clear route.");
    specialist->follower_of = controller;
    specialist->route.clear();
    specialist->awareness = Awareness::routine;
    mission_.acquired = true;
    push_event("specialist_acquired", controller, specialist_id, "Escort fixture; resistance and neural override are not implemented.");
    return Result::success();
}
Result World::extract() {
    if (mission_.state == MissionState::succeeded && mission_.reward_applied) return Result::success();
    if (mission_.state != MissionState::active) return Result::failure("Mission is no longer active.");
    const auto* target = actor(mission_.specialist);
    if (!mission_.acquired || !target || !target->alive()) return Result::failure("Specialist has not been acquired or is dead.");
    if (distance(target->position, mission_.extraction) > mission_.extraction_radius)
        return Result::failure("Specialist has not reached extraction.");
    std::size_t survivors = 0;
    for (const auto& pair : actors_) if (pair.second.kind == Kind::operative && pair.second.alive()) {
        ++survivors;
        if (distance(pair.second.position, mission_.extraction) > mission_.extraction_radius)
            return Result::failure("Every surviving operative must reach extraction.");
    }
    if (!survivors) return Result::failure("No surviving operatives.");
    mission_.state = MissionState::succeeded;
    mission_.outcome = "Specialist and surviving squad extracted.";
    if (!mission_.reward_applied) { credits_ += mission_.reward; mission_.reward_applied = true; }
    push_event("mission_succeeded", mission_.specialist, no_entity, mission_.outcome);
    return Result::success();
}
void World::abort() {
    if (mission_.state != MissionState::active) return;
    mission_.state = MissionState::aborted; mission_.outcome = "Executive aborted operation.";
    for (auto& pair : actors_) { pair.second.orders.clear(); pair.second.route.clear(); }
    for (auto& pair : vehicles_) pair.second.route.clear();
    push_event("mission_aborted", no_entity, no_entity, mission_.outcome);
}
void World::check_failure() {
    if (mission_.state != MissionState::active || mission_.specialist == no_entity) return;
    const auto* specialist = actor(mission_.specialist);
    if (!specialist || !specialist->alive()) {
        mission_.state = MissionState::failed; mission_.outcome = "Research specialist lost.";
    } else {
        const bool surviving = std::any_of(actors_.begin(), actors_.end(), [](const auto& p) {
            return p.second.kind == Kind::operative && p.second.alive();
        });
        if (!surviving) { mission_.state = MissionState::failed; mission_.outcome = "All deployed operatives lost."; }
    }
    if (mission_.state == MissionState::failed) push_event("mission_failed", mission_.specialist, no_entity, mission_.outcome);
}
Result World::begin_move(Actor& a, Vec2 destination) {
    const auto path = grid_.path(Grid::cell_at(a.position), Grid::cell_at(destination));
    if (path.empty()) return Result::failure("Navigation route is unavailable.");
    a.route.assign(path.begin() + 1, path.end());
    if (path.size() == 1 && distance(a.position, Grid::center(path.back())) > epsilon) a.route.push_back(path.back());
    return Result::success();
}
void World::update_vehicle(Vehicle& v, double dt) {
    const auto* driver = actor(v.occupants[0]);
    if (v.destroyed() || !driver || !driver->alive()) { v.route.clear(); return; }
    if (v.route.empty()) return;
    const Vec2 next = toward(v.position, Grid::center(v.route.front()), v.speed * dt);
    if (!grid_.clear_line(v.position, next)) { v.route.clear(); push_event("vehicle_blocked", v.id, no_entity); return; }
    for (const auto& pair : actors_) {
        const auto& a = pair.second;
        if (a.alive() && !a.vehicle && distance(next, a.position) < 0.85) return;
    }
    for (const auto& pair : vehicles_) if (pair.first != v.id && distance(next, pair.second.position) < 1.3) return;
    v.position = next;
    for (const auto id : v.occupants) if (id) actor(id)->position = next;
    if (distance(next, Grid::center(v.route.front())) < epsilon) v.route.pop_front();
}
void World::update_actor(Actor& a, double dt) {
    a.fire_cooldown = std::max(0.0, a.fire_cooldown - dt);
    if (!a.alive()) return;
    if (a.vehicle) {
        if (a.orders.empty()) return;
        const auto order = a.orders.front();
        auto result = Result::success();
        bool completed = true;
        if (order.kind == OrderKind::drive) {
            const auto* v = vehicle(a.vehicle);
            if (v->route.empty() && distance(v->position, order.destination) > 0.05) result = drive(a.id, order.destination);
            completed = !result.ok || (v->route.empty() && distance(v->position, order.destination) <= 0.05);
        } else if (order.kind == OrderKind::disembark) result = disembark(a.id);
        else if (order.kind == OrderKind::extract) result = extract();
        else result = Result::failure("This order requires disembarking first.");
        if (!result.ok) push_event("order_unavailable", a.id, order.target, result.reason);
        if (completed && !a.orders.empty()) a.orders.pop_front();
        return;
    }
    // Followers lose their controller explicitly and may be acquired again.
    if (a.follower_of) {
        const auto* controller = actor(a.follower_of);
        if (!controller || !controller->alive()) {
            a.follower_of = no_entity; a.route.clear();
            if (a.id == mission_.specialist) mission_.acquired = false;
            push_event("controller_lost", a.id, no_entity);
        } else if (a.route.empty() && distance(a.position, controller->position) > 1.0) {
            (void)begin_move(a, controller->position);
        }
    }
    if (a.kind == Kind::civilian && a.awareness == Awareness::fleeing && a.route.empty()) {
        Vec2 best = a.position;
        double best_distance = distance(best, a.last_known_threat);
        const Cell origin = Grid::cell_at(a.position);
        for (int y = -3; y <= 3; ++y) for (int x = -3; x <= 3; ++x) {
            const Cell c{origin.x + x, origin.y + y};
            if (!grid_.walkable(c) || grid_.path(origin, c).empty()) continue;
            if (distance(Grid::center(c), a.last_known_threat) > best_distance) {
                best = Grid::center(c); best_distance = distance(best, a.last_known_threat);
            }
        }
        (void)begin_move(a, best);
    }
    if (a.kind == Kind::security) {
        for (const auto& pair : actors_) {
            const auto& other = pair.second;
            if (other.kind == Kind::operative && other.alive() && !other.holstered && !other.vehicle &&
                distance(a.position, other.position) <= 10.0 && grid_.clear_line(a.position, other.position)) {
                a.awareness = Awareness::engaged; a.last_seen_enemy = other.id; a.last_known_threat = other.position;
                break;
            }
        }
        const auto* enemy = actor(a.last_seen_enemy);
        if (enemy && enemy->alive() && a.awareness == Awareness::engaged) {
            const auto shot = shoot(a.id, enemy->id);
            if (!shot.ok && grid_.clear_line(a.position, enemy->position) && a.route.empty())
                (void)begin_move(a, enemy->position);
            if (!grid_.clear_line(a.position, enemy->position)) {
                a.awareness = Awareness::investigating;
                if (a.route.empty()) (void)begin_move(a, a.last_known_threat);
            }
        } else if (a.awareness == Awareness::investigating && a.route.empty()) {
            (void)begin_move(a, a.last_known_threat);
        }
    }
    if (!a.orders.empty()) {
        const auto order = a.orders.front();
        Result result = Result::success();
        bool completed = true;
        switch (order.kind) {
        case OrderKind::move:
            if (a.route.empty() && distance(a.position, order.destination) > 0.05) result = begin_move(a, order.destination);
            completed = !result.ok || (a.route.empty() && distance(a.position, order.destination) <= 0.05);
            break;
        case OrderKind::attack: {
            const auto* target = actor(order.target);
            if (!target || !target->alive()) break;
            const auto shot = shoot(a.id, order.target);
            if (!shot.ok && a.fire_cooldown <= epsilon) push_event("order_unavailable", a.id, order.target, shot.reason);
            completed = !target->alive(); break;
        }
        case OrderKind::board: {
            const auto* v = vehicle(order.target);
            if (!v || v->destroyed()) result = Result::failure("Vehicle is unavailable.");
            else if (distance(a.position, v->position) <= 1.5) result = board(a.id, order.target);
            else { result = begin_move(a, v->position); completed = !result.ok; }
            break;
        }
        case OrderKind::drive: result = drive(a.id, order.destination); break;
        case OrderKind::disembark: result = disembark(a.id); break;
        case OrderKind::acquire: result = acquire(a.id, order.target); break;
        case OrderKind::extract: result = extract(); break;
        case OrderKind::stop: a.route.clear(); break;
        }
        if (!result.ok) push_event("order_unavailable", a.id, order.target, result.reason);
        // Boarding/disembarking may already have cleared the actor's queue.
        if (completed && !a.orders.empty()) { a.orders.pop_front(); if (order.kind == OrderKind::move) a.route.clear(); }
    }
    if (a.route.empty() || a.vehicle) return;
    const Vec2 next = toward(a.position, Grid::center(a.route.front()), a.speed * dt);
    if (!grid_.clear_line(a.position, next)) {
        a.route.clear();
        push_event("route_obstructed", a.id, no_entity, "Route will be recomputed on the next order tick.");
        return;
    }
    bool occupied = false;
    for (const auto& pair : vehicles_) if (distance(next, pair.second.position) < 0.85) occupied = true;
    for (const auto& pair : actors_)
        if (pair.first != a.id && pair.second.alive() && !pair.second.vehicle && distance(next, pair.second.position) < 0.6) occupied = true;
    if (occupied) {
        if (!a.orders.empty() && a.orders.front().kind == OrderKind::move) {
            Grid navigation = grid_;
            for (const auto& pair : vehicles_) navigation.set_blocked(Grid::cell_at(pair.second.position), true);
            for (const auto& pair : actors_) if (pair.first != a.id && pair.second.alive() && !pair.second.vehicle)
                navigation.set_blocked(Grid::cell_at(pair.second.position), true);
            const auto alternate = navigation.path(Grid::cell_at(a.position), Grid::cell_at(a.orders.front().destination));
            if (!alternate.empty()) a.route.assign(alternate.begin() + 1, alternate.end());
        }
        return;
    }
    a.position = next;
    if (distance(a.position, Grid::center(a.route.front())) < epsilon) a.route.pop_front();
}
void World::tick(double dt) {
    time_ += dt;
    for (auto& pair : vehicles_) update_vehicle(pair.second, dt);
    for (auto& pair : actors_) update_actor(pair.second, dt);
    check_failure();
}
void World::advance(double elapsed) {
    if (!std::isfinite(elapsed) || elapsed < 0.0 || elapsed > 60.0) throw std::invalid_argument("Elapsed time must be finite and in [0,60] seconds.");
    if (paused || mission_.state != MissionState::active) return;
    accumulator_ += elapsed;
    while (accumulator_ + epsilon >= tick_seconds && mission_.state == MissionState::active) {
        accumulator_ = std::max(0.0, accumulator_ - tick_seconds);
        tick(tick_seconds);
    }
    if (mission_.state != MissionState::active) accumulator_ = 0.0;
}

void World::write_snapshot(std::ostream& out) const {
    out << std::setprecision(17) << "BLACKGLASS_CORE 1\n";
    out << grid_.width_ << ' ' << grid_.height_ << '\n';
    for (const auto cell : grid_.blocked_) out << static_cast<int>(cell) << ' ';
    out << '\n' << next_id_ << ' ' << time_ << ' ' << accumulator_ << ' ' << credits_ << ' ' << friendly_fire << ' ' << paused << '\n';
    out << weapons_.size() << '\n';
    for (const auto& pair : weapons_) {
        const auto& w = pair.second;
        out << std::quoted(w.id) << ' ' << w.range << ' ' << w.damage << ' ' << w.interval_seconds << ' ' << w.sound_radius << ' ' << w.weight << '\n';
    }
    out << actors_.size() << '\n';
    for (const auto& pair : actors_) {
        const auto& a = pair.second;
        out << a.id << ' ' << static_cast<int>(a.kind) << ' ' << a.position.x << ' ' << a.position.y << ' ' << a.health << ' ' << a.armor << ' '
            << a.speed << ' ' << a.capacity << ' ' << a.fire_cooldown << ' ' << a.selected << ' ' << a.holstered << ' '
            << static_cast<int>(a.awareness) << ' ' << a.vehicle << ' ' << a.follower_of << ' ' << a.last_seen_enemy << ' '
            << a.last_known_threat.x << ' ' << a.last_known_threat.y << ' ' << a.active_slot << '\n';
        for (const auto& slot : a.inventory) out << std::quoted(slot.definition) << ' ' << slot.ammunition << '\n';
        out << a.orders.size() << '\n';
        for (const auto& o : a.orders) out << static_cast<int>(o.kind) << ' ' << o.destination.x << ' ' << o.destination.y << ' ' << o.target << '\n';
        out << a.route.size() << '\n';
        for (const auto c : a.route) out << c.x << ' ' << c.y << '\n';
    }
    out << vehicles_.size() << '\n';
    for (const auto& pair : vehicles_) {
        const auto& v = pair.second;
        out << v.id << ' ' << v.position.x << ' ' << v.position.y << ' ' << v.health << ' ' << v.speed;
        for (const auto id : v.occupants) out << ' ' << id;
        out << '\n' << v.route.size() << '\n';
        for (const auto c : v.route) out << c.x << ' ' << c.y << '\n';
    }
    out << static_cast<int>(mission_.state) << ' ' << mission_.specialist << ' ' << mission_.extraction.x << ' '
        << mission_.extraction.y << ' ' << mission_.extraction_radius << ' ' << mission_.acquired << ' '
        << mission_.reward_applied << ' ' << mission_.reward << ' ' << std::quoted(mission_.outcome) << '\n';
}
Result World::read_snapshot(std::istream& in) {
    try {
        std::string magic; int version = 0, width = 0, height = 0;
        if (!(in >> magic >> version) || magic != "BLACKGLASS_CORE" || version != 1)
            return Result::failure("Missing or incompatible core save version.");
        if (!(in >> width >> height)) return Result::failure("Invalid grid header.");
        World candidate(Grid(width, height));
        for (auto& cell : candidate.grid_.blocked_) {
            int value = -1; if (!(in >> value) || value < 0 || value > 1) return Result::failure("Invalid grid cell.");
            cell = static_cast<std::uint8_t>(value);
        }
        if (!(in >> candidate.next_id_ >> candidate.time_ >> candidate.accumulator_ >> candidate.credits_ >> candidate.friendly_fire >> candidate.paused))
            return Result::failure("Invalid core clock or financial data.");
        std::size_t count = 0;
        if (!(in >> count) || count > 256) return Result::failure("Invalid weapon count.");
        for (std::size_t n = 0; n < count; ++n) {
            WeaponDefinition w;
            if (!(in >> std::quoted(w.id) >> w.range >> w.damage >> w.interval_seconds >> w.sound_radius >> w.weight) || !candidate.weapons_.emplace(w.id, w).second)
                return Result::failure("Invalid or duplicate weapon definition.");
        }
        if (!(in >> count) || count > 2048) return Result::failure("Invalid actor count.");
        for (std::size_t n = 0; n < count; ++n) {
            Actor a; int kind = -1, awareness = -1;
            if (!(in >> a.id >> kind >> a.position.x >> a.position.y >> a.health >> a.armor >> a.speed >> a.capacity >> a.fire_cooldown >>
                a.selected >> a.holstered >> awareness >> a.vehicle >> a.follower_of >> a.last_seen_enemy >>
                a.last_known_threat.x >> a.last_known_threat.y >> a.active_slot) || !valid_kind(kind) ||
                awareness < 0 || awareness > static_cast<int>(Awareness::fleeing))
                return Result::failure("Invalid actor state.");
            a.kind = static_cast<Kind>(kind); a.awareness = static_cast<Awareness>(awareness);
            for (auto& slot : a.inventory) if (!(in >> std::quoted(slot.definition) >> slot.ammunition)) return Result::failure("Invalid inventory slot.");
            std::size_t orders = 0, route = 0;
            if (!(in >> orders) || orders > 256) return Result::failure("Invalid order count.");
            for (std::size_t j = 0; j < orders; ++j) {
                Order o; int type = -1;
                if (!(in >> type >> o.destination.x >> o.destination.y >> o.target) || !valid_order(type)) return Result::failure("Invalid queued order.");
                o.kind = static_cast<OrderKind>(type); a.orders.push_back(o);
            }
            if (!(in >> route) || route > candidate.grid_.blocked_.size()) return Result::failure("Invalid actor route length.");
            for (std::size_t j = 0; j < route; ++j) { Cell c; if (!(in >> c.x >> c.y)) return Result::failure("Invalid actor route."); a.route.push_back(c); }
            if (!candidate.actors_.emplace(a.id, std::move(a)).second) return Result::failure("Duplicate actor identity.");
        }
        if (!(in >> count) || count > 1024) return Result::failure("Invalid vehicle count.");
        for (std::size_t n = 0; n < count; ++n) {
            Vehicle v;
            if (!(in >> v.id >> v.position.x >> v.position.y >> v.health >> v.speed)) return Result::failure("Invalid vehicle state.");
            for (auto& occupant : v.occupants) if (!(in >> occupant)) return Result::failure("Invalid seat identity.");
            std::size_t route = 0;
            if (!(in >> route) || route > candidate.grid_.blocked_.size()) return Result::failure("Invalid vehicle route length.");
            for (std::size_t j = 0; j < route; ++j) { Cell c; if (!(in >> c.x >> c.y)) return Result::failure("Invalid vehicle route."); v.route.push_back(c); }
            if (!candidate.vehicles_.emplace(v.id, std::move(v)).second) return Result::failure("Duplicate vehicle identity.");
        }
        int state = -1;
        auto& m = candidate.mission_;
        if (!(in >> state >> m.specialist >> m.extraction.x >> m.extraction.y >> m.extraction_radius >>
            m.acquired >> m.reward_applied >> m.reward >> std::quoted(m.outcome)) ||
            state < 0 || state > static_cast<int>(MissionState::aborted)) return Result::failure("Invalid mission state.");
        m.state = static_cast<MissionState>(state);
        in >> std::ws;
        if (!in.eof()) return Result::failure("Unexpected trailing save data.");
        const auto valid = candidate.validate();
        if (!valid.ok) return valid;
        *this = std::move(candidate);
        return Result::success();
    } catch (const std::exception& e) { return Result::failure(std::string("Invalid save: ") + e.what()); }
}

Result World::validate() const {
    EntityId maximum = 0;
    if (!std::isfinite(time_) || time_ < 0 || !std::isfinite(accumulator_) || accumulator_ < 0 ||
        accumulator_ >= tick_seconds + epsilon || credits_ < 0) return Result::failure("Invalid simulation clock or finances.");
    for (const auto& pair : weapons_) {
        const auto& w = pair.second;
        if (w.id.empty() || w.id.size() > 64 || pair.first != w.id || !std::isfinite(w.range) || w.range <= 0 || w.range > 1000 ||
            !std::isfinite(w.damage) || w.damage <= 0 || w.damage > 10000 ||
            !std::isfinite(w.interval_seconds) || w.interval_seconds < tick_seconds || w.interval_seconds > 60 ||
            !std::isfinite(w.sound_radius) || w.sound_radius < 0 || w.sound_radius > 1000 ||
            !std::isfinite(w.weight) || w.weight <= 0 || w.weight > 1000)
            return Result::failure("Invalid saved weapon definition.");
    }
    std::size_t operative_count = 0;
    std::set<EntityId> seated;
    for (const auto& pair : vehicles_) {
        const auto& v = pair.second;
        if (!pair.first || pair.first != v.id || actors_.count(v.id) || !finite(v.position) || !grid_.walkable(Grid::cell_at(v.position)) ||
            !std::isfinite(v.health) || v.health < 0 || v.health > 10000 || !std::isfinite(v.speed) || v.speed <= 0 || v.speed > 100)
            return Result::failure("Invalid vehicle identity or state.");
        maximum = std::max(maximum, v.id);
        for (const auto id : v.occupants) if (id) {
            const auto* a = actor(id);
            if (!a || a->vehicle != v.id || !seated.insert(id).second || distance(a->position, v.position) > epsilon)
                return Result::failure("Inconsistent or duplicated vehicle occupant.");
        }
        for (const auto c : v.route) if (!grid_.inside(c)) return Result::failure("Vehicle route leaves the map.");
    }
    for (const auto& pair : actors_) {
        const auto& a = pair.second;
        if (!pair.first || pair.first != a.id || !valid_kind(static_cast<int>(a.kind)) ||
            !finite(a.position) || !finite(a.last_known_threat) || !grid_.walkable(Grid::cell_at(a.position)) ||
            !std::isfinite(a.health) || a.health < 0 || a.health > 10000 ||
            !std::isfinite(a.armor) || a.armor < 0 || a.armor > 10000 ||
            !std::isfinite(a.speed) || a.speed <= 0 || a.speed > 100 ||
            !std::isfinite(a.capacity) || a.capacity < 0 || a.capacity > 10000 ||
            !std::isfinite(a.fire_cooldown) || a.fire_cooldown < 0 || a.fire_cooldown > 60 || a.active_slot >= a.inventory.size())
            return Result::failure("Invalid operative/NPC identity or values.");
        maximum = std::max(maximum, a.id);
        if (a.kind == Kind::operative) ++operative_count;
        if (a.vehicle && (!vehicle(a.vehicle) || !seated.count(a.id))) return Result::failure("Actor has an invalid seat relationship.");
        if (a.follower_of && (!actor(a.follower_of) || actor(a.follower_of)->kind != Kind::operative || a.follower_of == a.id))
            return Result::failure("Follower has an invalid controller.");
        if (a.last_seen_enemy && !actor(a.last_seen_enemy)) return Result::failure("Awareness references a missing entity.");
        for (const auto& slot : a.inventory) {
            if ((!slot.definition.empty() && !weapons_.count(slot.definition)) || slot.ammunition < 0 || slot.ammunition > 100000 ||
                (slot.definition.empty() && slot.ammunition != 0)) return Result::failure("Inventory references invalid equipment.");
        }
        if (carried_weight(a) > a.capacity + epsilon) return Result::failure("Saved inventory exceeds carrying capacity.");
        for (const auto& o : a.orders) {
            if (!valid_order(static_cast<int>(o.kind)) || !finite(o.destination) ||
                (o.kind == OrderKind::attack && !actor(o.target)) || (o.kind == OrderKind::acquire && !actor(o.target)) ||
                (o.kind == OrderKind::board && !vehicle(o.target))) return Result::failure("Order references invalid state.");
        }
        for (const auto c : a.route) if (!grid_.inside(c)) return Result::failure("Actor route leaves the map.");
    }
    if (operative_count > 4 || next_id_ <= maximum) return Result::failure("Invalid squad size or next stable identity.");
    const auto& m = mission_;
    if (!finite(m.extraction) || !std::isfinite(m.extraction_radius) || m.extraction_radius <= 0 || m.extraction_radius > 100 ||
        m.reward < 0 || m.reward > 1000000 || m.outcome.size() > 4096) return Result::failure("Invalid mission parameters.");
    if (m.specialist && (!actor(m.specialist) || actor(m.specialist)->kind != Kind::specialist))
        return Result::failure("Mission references a missing specialist.");
    if (m.acquired && (!actor(m.specialist) || !actor(m.specialist)->follower_of))
        return Result::failure("Acquisition is inconsistent with follower state.");
    if ((m.reward_applied && m.state != MissionState::succeeded) ||
        (m.state == MissionState::succeeded && (!m.reward_applied || credits_ < m.reward)))
        return Result::failure("Mission settlement is inconsistent with finances.");
    return Result::success();
}
Result World::save(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid.ok) return Result::failure("Cannot save invalid state: " + valid.reason);
    try {
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        const auto temporary = std::filesystem::path(path.native() + std::filesystem::path(".tmp").native());
        const auto previous = std::filesystem::path(path.native() + std::filesystem::path(".previous").native());
        {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            if (!out) return Result::failure("Cannot create temporary save.");
            write_snapshot(out); out.flush();
            if (!out) return Result::failure("Save write failed; original save retained.");
            out.close();
            if (!out) return Result::failure("Save close failed; original save retained.");
        }
        if (std::filesystem::exists(path)) {
            std::ifstream current(path, std::ios::binary);
            World existing(Grid(1,1));
            if (existing.read_snapshot(current).ok)
                std::filesystem::copy_file(path, previous, std::filesystem::copy_options::overwrite_existing);
        }
        return replace_file(temporary, path);
    } catch (const std::exception& e) { return Result::failure(std::string("Save failed: ") + e.what()); }
}
Result World::load(const std::filesystem::path& path) {
    try {
        if (!std::filesystem::exists(path)) return Result::failure("Save slot does not exist.");
        if (std::filesystem::file_size(path) > 16 * 1024 * 1024) return Result::failure("Save exceeds 16 MiB limit.");
        std::ifstream input(path, std::ios::binary);
        if (!input) return Result::failure("Save slot cannot be opened.");
        return read_snapshot(input);
    } catch (const std::exception& e) { return Result::failure(std::string("Load failed: ") + e.what()); }
}
} // namespace blackglass
