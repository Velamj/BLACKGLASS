#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace blackglass {
using EntityId = std::uint64_t;
constexpr EntityId no_entity = 0;
struct Vec2 {
    double x = 0.0, y = 0.0;
};
double distance(Vec2 a, Vec2 b);
struct Cell {
    int x = 0, y = 0;
    bool operator==(Cell rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(Cell rhs) const { return !(*this == rhs); }
};
struct Result {
    bool ok = false;
    std::string reason;
    static Result success();
    static Result failure(std::string reason);
};
// A small 2D navigation fixture for core contracts. Unreal navigation and
// accessible floors are still required for the game.
class Grid {
public:
    Grid(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    bool inside(Cell cell) const;
    bool walkable(Cell cell) const;
    void set_blocked(Cell cell, bool blocked);
    std::vector<Cell> path(Cell start, Cell goal) const;
    bool clear_line(Vec2 from, Vec2 to) const;
    static Cell cell_at(Vec2 position);
    static Vec2 center(Cell cell);
private:
    int width_, height_;
    std::vector<std::uint8_t> blocked_;
    friend class World;
};
struct WeaponDefinition {
    std::string id;
    double range = 0.0;
    double damage = 0.0;
    double interval_seconds = 0.0;
    double sound_radius = 0.0;
    double weight = 0.0;
};
// Strict, simple CSV format: id,range,damage,interval,sound_radius,weight.
// Values are in metres, hit points, seconds and arbitrary carrying units.
Result read_weapons(std::istream& input, std::map<std::string, WeaponDefinition>& output);
enum class Kind { operative, civilian, security, specialist };
enum class OrderKind { move, attack, stop, board, drive, disembark, acquire, extract };
enum class MissionState { active, succeeded, failed, aborted };
enum class Awareness { routine, investigating, engaged, fleeing };
struct Order {
    OrderKind kind = OrderKind::stop;
    Vec2 destination;
    EntityId target = no_entity;
};
struct InventorySlot {
    std::string definition;
    int ammunition = 0;
};
struct Actor {
    EntityId id = no_entity;
    Kind kind = Kind::civilian;
    Vec2 position;
    double health = 100.0;
    double armor = 0.0;
    double speed = 2.0;
    double capacity = 24.0;
    double fire_cooldown = 0.0;
    bool selected = false;
    bool holstered = true;
    Awareness awareness = Awareness::routine;
    EntityId vehicle = no_entity;
    EntityId follower_of = no_entity;
    EntityId last_seen_enemy = no_entity;
    Vec2 last_known_threat;
    std::array<InventorySlot, 8> inventory{};
    std::size_t active_slot = 0;
    std::deque<Order> orders;
    std::deque<Cell> route;
    bool alive() const { return health > 0.0; }
};
struct Vehicle {
    EntityId id = no_entity;
    Vec2 position;
    double health = 180.0;
    double speed = 5.0;
    std::array<EntityId, 4> occupants{};
    std::deque<Cell> route;
    bool destroyed() const { return health <= 0.0; }
};
struct Mission {
    MissionState state = MissionState::active;
    EntityId specialist = no_entity;
    Vec2 extraction;
    double extraction_radius = 1.25;
    bool acquired = false;
    bool reward_applied = false;
    int reward = 500;
    std::string outcome;
};
struct Event {
    double time = 0.0;
    std::string type;
    EntityId source = no_entity, target = no_entity;
    std::string detail;
};
class World {
public:
    explicit World(Grid grid);
    EntityId add_actor(Kind kind, Vec2 position);
    EntityId add_vehicle(Vec2 position);
    Actor* actor(EntityId id);
    const Actor* actor(EntityId id) const;
    Vehicle* vehicle(EntityId id);
    const Vehicle* vehicle(EntityId id) const;
    const std::map<EntityId, Actor>& actors() const { return actors_; }
    const std::map<EntityId, Vehicle>& vehicles() const { return vehicles_; }
    Grid& grid() { return grid_; }
    const Grid& grid() const { return grid_; }
    std::map<std::string, WeaponDefinition>& weapons() { return weapons_; }
    const std::map<std::string, WeaponDefinition>& weapons() const { return weapons_; }
    Mission& mission() { return mission_; }
    const Mission& mission() const { return mission_; }
    const std::vector<Event>& events() const { return events_; }
    int credits() const { return credits_; }
    double time() const { return time_; }
    bool friendly_fire = false;
    bool paused = false;

    Result select(const std::vector<EntityId>& ids, bool additive = false);
    Result issue(const std::vector<EntityId>& ids, Order order, bool queued = false);
    Result equip(EntityId actor, std::size_t slot, const std::string& weapon, int ammunition);
    Result switch_weapon(EntityId actor, std::size_t slot);
    Result holster(EntityId actor, bool holstered);
    Result transfer(EntityId from, std::size_t from_slot, EntityId to, std::size_t to_slot);
    Result shoot(EntityId attacker, EntityId target);
    Result board(EntityId actor, EntityId vehicle);
    Result disembark(EntityId actor);
    Result drive(EntityId driver, Vec2 destination);
    Result damage_vehicle(EntityId vehicle, double damage);
    Result acquire(EntityId controller, EntityId specialist);
    Result extract();
    void abort();
    // Accumulator executes 20 ms simulation ticks. Pause advances no clocks.
    void advance(double elapsed_seconds);
    Result save(const std::filesystem::path& path) const;
    // Parse and validate into a temporary World before replacing this instance.
    Result load(const std::filesystem::path& path);
    void write_snapshot(std::ostream& stream) const;
    Result read_snapshot(std::istream& stream);
private:
    Grid grid_;
    EntityId next_id_ = 1;
    std::map<EntityId, Actor> actors_;
    std::map<EntityId, Vehicle> vehicles_;
    std::map<std::string, WeaponDefinition> weapons_;
    Mission mission_;
    std::vector<Event> events_;
    double time_ = 0.0, accumulator_ = 0.0;
    int credits_ = 0;
    void tick(double dt);
    void notice_shot(EntityId source, Vec2 origin, double sound_radius);
    void push_event(std::string type, EntityId source, EntityId target, std::string detail = {});
    Result begin_move(Actor& actor, Vec2 destination);
    void update_actor(Actor& actor, double dt);
    void update_vehicle(Vehicle& vehicle, double dt);
    void check_failure();
    Result validate() const;
    double carried_weight(const Actor& actor) const;
};
} // namespace blackglass
