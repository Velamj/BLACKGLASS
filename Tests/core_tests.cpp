#include "blackglass/core.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace blackglass;
#define CHECK(expression) do { if (!(expression)) throw std::runtime_error(std::string("Line ") + std::to_string(__LINE__) + ": " + #expression); } while (false)
#define OK(expression) do { const Result result_ = (expression); if (!result_.ok) throw std::runtime_error(std::string("Line ") + std::to_string(__LINE__) + ": " + result_.reason); } while (false)

World fixture(int width = 32, int height = 16) {
    World world(Grid(width, height));
    std::ifstream definitions("Data/Weapons.csv");
    OK(read_weapons(definitions, world.weapons()));
    return world;
}
std::string snapshot(const World& world) { std::ostringstream out; world.write_snapshot(out); return out.str(); }
World restored(const World& world) {
    World loaded(Grid(1,1)); std::istringstream input(snapshot(world));
    OK(loaded.read_snapshot(input)); return loaded;
}
void close(Vec2 a, Vec2 b) { CHECK(distance(a,b) < 1e-6); }
void select_and_group_move() {
    auto world = fixture();
    std::vector<EntityId> ids;
    for (int y : {1,3,5,7}) ids.push_back(world.add_actor(Kind::operative, {1.5, y + 0.5}));
    OK(world.select(ids));
    for (const auto id : ids) CHECK(world.actor(id)->selected);
    OK(world.issue(ids, {OrderKind::move, {8.5,4.5}, no_entity}));
    std::vector<Vec2> goals;
    std::set<std::pair<double,double>> unique;
    for (const auto id : ids) {
        goals.push_back(world.actor(id)->orders.front().destination);
        unique.insert({goals.back().x, goals.back().y});
    }
    CHECK(unique.size() == 4);
    world.advance(15.0);
    for (std::size_t i = 0; i < ids.size(); ++i) close(world.actor(ids[i])->position, goals[i]);
    const auto before = snapshot(world);
    CHECK(!world.select({999}).ok); CHECK(snapshot(world) == before);
}
void individual_queue_and_stop() {
    auto world = fixture();
    const auto a = world.add_actor(Kind::operative, {1.5,1.5});
    const auto b = world.add_actor(Kind::operative, {1.5,7.5});
    OK(world.issue({a}, {OrderKind::move,{4.5,1.5},0}));
    OK(world.issue({a}, {OrderKind::move,{4.5,4.5},0}, true));
    world.advance(4.0);
    close(world.actor(a)->position,{4.5,4.5}); close(world.actor(b)->position,{1.5,7.5});
    OK(world.issue({a}, {OrderKind::move,{10.5,4.5},0}));
    world.advance(0.4);
    OK(world.issue({a}, {OrderKind::stop,{},0}));
    const auto stopped = world.actor(a)->position; world.advance(1.0); close(world.actor(a)->position, stopped);
}
void doorway_changes_navigation() {
    auto world = fixture(12,9);
    for (int y = 0; y < 9; ++y) world.grid().set_blocked({5,y},true);
    const auto a = world.add_actor(Kind::operative,{1.5,4.5});
    CHECK(!world.issue({a},{OrderKind::move,{9.5,4.5},0}).ok);
    world.grid().set_blocked({5,4},false);
    OK(world.issue({a},{OrderKind::move,{9.5,4.5},0}));
    world.advance(1.0); world.grid().set_blocked({5,4},true); world.advance(2.0);
    CHECK(world.actor(a)->position.x < 5.0);
    CHECK(!world.events().empty());
    world.grid().set_blocked({5,4},false);
    OK(world.issue({a},{OrderKind::move,{9.5,4.5},0})); world.advance(6.0);
    close(world.actor(a)->position,{9.5,4.5});
}
void narrow_route_and_traffic_yield() {
    auto world = fixture(12,5);
    for (int y : {0,1,3,4}) for (int x = 0; x < 12; ++x) world.grid().set_blocked({x,y},true);
    const auto a = world.add_actor(Kind::operative,{1.5,2.5});
    const auto driver = world.add_actor(Kind::operative,{5.5,2.5});
    const auto car = world.add_vehicle({6.5,2.5});
    OK(world.board(driver,car));
    OK(world.issue({a},{OrderKind::move,{9.5,2.5},0})); world.advance(2.0);
    CHECK(world.actor(a)->position.x < 6.0);
    OK(world.drive(driver,{11.5,2.5})); world.advance(6.0);
    close(world.actor(a)->position,{9.5,2.5});
}
void blocked_shot_preserves_ammunition() {
    auto world = fixture();
    const auto a = world.add_actor(Kind::operative,{1.5,2.5});
    const auto enemy = world.add_actor(Kind::security,{7.5,2.5});
    OK(world.equip(a,0,"compact_sidearm",10));
    world.grid().set_blocked({4,2},true);
    const auto blocked = world.shoot(a,enemy);
    CHECK(!blocked.ok && blocked.reason.find("geometry") != std::string::npos);
    CHECK(world.actor(enemy)->health == 100); CHECK(world.actor(a)->inventory[0].ammunition == 10);
    world.grid().set_blocked({4,2},false); OK(world.shoot(a,enemy));
    CHECK(world.actor(enemy)->health == 82); CHECK(world.actor(a)->inventory[0].ammunition == 9);
    CHECK(world.events().back().type == "hit" && world.events().back().target == enemy);
}
void corners_block_and_clear_rays() {
    Grid grid(8,8); grid.set_blocked({2,1},true);
    CHECK(!grid.clear_line({1.5,1.5},{3.5,3.5}));
    grid.set_blocked({2,1},false);
    CHECK(grid.clear_line({1.5,1.5},{3.5,3.5}));
    CHECK(grid.clear_line({3.5,3.5},{1.5,1.5}));
    CHECK(grid.clear_line({3.5,3.5},{3.5,3.5}));
}
void friendly_fire_models_actual_interceptor() {
    auto world = fixture();
    const auto a = world.add_actor(Kind::operative,{1.5,2.5});
    const auto friend_id = world.add_actor(Kind::operative,{3.5,2.5});
    const auto enemy = world.add_actor(Kind::security,{7.5,2.5});
    OK(world.equip(a,0,"compact_sidearm",10));
    CHECK(!world.shoot(a,enemy).ok); CHECK(world.actor(a)->inventory[0].ammunition == 10);
    world.friendly_fire = true; OK(world.shoot(a,enemy));
    CHECK(world.actor(friend_id)->health == 82); CHECK(world.actor(enemy)->health == 100);
    CHECK(world.events().back().target == friend_id);
}
void armor_and_weapon_roles() {
    auto world = fixture();
    const auto a = world.add_actor(Kind::operative,{1.5,2.5});
    const auto enemy = world.add_actor(Kind::security,{16.5,2.5});
    OK(world.equip(a,0,"compact_sidearm",10));
    OK(world.equip(a,1,"compact_automatic",10));
    CHECK(!world.shoot(a,enemy).ok); OK(world.switch_weapon(a,1));
    world.actor(enemy)->armor = 4; OK(world.shoot(a,enemy));
    CHECK(world.actor(enemy)->health == 95);
    CHECK(!world.shoot(a,enemy).ok);
    world.advance(0.12); OK(world.shoot(a,enemy));
    CHECK(world.actor(a)->inventory[1].ammunition == 8);
}
void security_responds_to_local_evidence() {
    auto world = fixture(40,16);
    const auto a = world.add_actor(Kind::operative,{1.5,2.5});
    const auto target = world.add_actor(Kind::security,{3.5,2.5});
    const auto witness = world.add_actor(Kind::security,{2.5,4.5});
    const auto hidden = world.add_actor(Kind::security,{6.5,2.5});
    const auto distant = world.add_actor(Kind::security,{30.5,2.5});
    const auto civilian = world.add_actor(Kind::civilian,{2.5,6.5});
    world.grid().set_blocked({4,2},true); OK(world.equip(a,0,"compact_sidearm",20));
    OK(world.shoot(a,target));
    CHECK(world.actor(witness)->awareness == Awareness::engaged);
    CHECK(world.actor(witness)->last_seen_enemy == a);
    CHECK(world.actor(hidden)->awareness == Awareness::investigating);
    CHECK(world.actor(hidden)->last_seen_enemy == no_entity);
    CHECK(world.actor(distant)->awareness == Awareness::routine);
    CHECK(world.actor(civilian)->awareness == Awareness::fleeing);
    OK(world.holster(a,true)); CHECK(world.actor(witness)->awareness == Awareness::engaged);
    const double start = distance(world.actor(civilian)->position,world.actor(a)->position);
    world.advance(0.4); CHECK(distance(world.actor(civilian)->position,world.actor(a)->position) > start);
}
void frame_partition_and_pause() {
    auto single = fixture();
    const auto a = single.add_actor(Kind::operative,{1.5,1.5});
    OK(single.issue({a},{OrderKind::move,{8.5,1.5},0}));
    auto many = single;
    single.advance(2.0);
    for (int i = 0; i < 120; ++i) many.advance(1.0 / 60.0);
    close(single.actor(a)->position, many.actor(a)->position);
    CHECK(std::abs(single.time() - many.time()) < 1e-8);
    const auto before = snapshot(many); many.paused = true;
    const auto paused_snapshot = snapshot(many); many.advance(10.0);
    CHECK(snapshot(many) == paused_snapshot); many.paused = false; CHECK(snapshot(many) == before);
}
void firing_cadence_partition() {
    auto single = fixture(); const auto a = single.add_actor(Kind::operative,{1.5,2.5});
    const auto enemy = single.add_actor(Kind::security,{8.5,2.5}); single.actor(enemy)->health = 1000;
    OK(single.equip(a,0,"compact_automatic",100));
    OK(single.issue({a},{OrderKind::attack,{},enemy}));
    auto many = single; single.advance(2.0);
    for (int i = 0; i < 60; ++i) many.advance(1.0 / 30.0);
    CHECK(single.actor(a)->inventory[0].ammunition == many.actor(a)->inventory[0].ammunition);
    CHECK(single.actor(enemy)->health == many.actor(enemy)->health);
    CHECK(single.actor(a)->inventory[0].ammunition < 90);
}
void inventory_transfer_is_transactional() {
    auto world = fixture(); const auto a = world.add_actor(Kind::operative,{1.5,1.5});
    const auto b = world.add_actor(Kind::operative,{2.5,1.5});
    OK(world.equip(a,0,"compact_automatic",36));
    world.actor(b)->capacity = 4;
    const auto before = snapshot(world); CHECK(!world.transfer(a,0,b,0).ok); CHECK(snapshot(world) == before);
    world.actor(b)->capacity = 24; OK(world.transfer(a,0,b,0));
    CHECK(world.actor(a)->inventory[0].definition.empty()); CHECK(world.actor(b)->inventory[0].ammunition == 36);
    CHECK(!world.equip(a,8,"compact_sidearm",10).ok);
    CHECK(!world.transfer(b,0,a,8).ok);
}
void vehicle_seats_and_driver_orders() {
    auto world = fixture();
    std::vector<EntityId> ids;
    for (const Vec2 p : std::vector<Vec2>{{3.5,4.5},{4.5,3.5},{5.5,4.5},{4.5,5.5}}) ids.push_back(world.add_actor(Kind::operative,p));
    const auto extra = world.add_actor(Kind::civilian,{3.5,4.5});
    const auto v = world.add_vehicle({4.5,4.5});
    for (const auto id : ids) OK(world.board(id,v));
    CHECK(!world.board(extra,v).ok); CHECK(!world.drive(ids[1],{8.5,4.5}).ok);
    OK(world.issue({ids[0]},{OrderKind::drive,{8.5,4.5},0}));
    world.advance(1.0); close(world.vehicle(v)->position,{8.5,4.5});
    for (const auto id : ids) close(world.actor(id)->position,world.vehicle(v)->position);
    OK(world.drive(ids[0],{12.5,4.5})); world.advance(0.1);
    OK(world.issue({ids[0]},{OrderKind::stop,{},0}));
    const auto stopped = world.vehicle(v)->position; world.advance(0.5); close(world.vehicle(v)->position,stopped);
    OK(world.issue({ids[1]},{OrderKind::disembark,{},0})); world.advance(0.02);
    CHECK(world.actor(ids[1])->vehicle == no_entity);
}
void blocked_exit_and_wreck_persist() {
    auto world = fixture(7,7); const auto a = world.add_actor(Kind::operative,{2.5,3.5});
    const auto v = world.add_vehicle({3.5,3.5}); OK(world.board(a,v));
    for (int y = 2; y <= 4; ++y) for (int x = 2; x <= 4; ++x)
        if (x != 3 || y != 3) world.grid().set_blocked({x,y},true);
    CHECK(!world.disembark(a).ok);
    OK(world.damage_vehicle(v,200));
    CHECK(world.vehicle(v)->destroyed()); CHECK(world.actor(a)->vehicle == v);
    CHECK(world.actor(a)->health == 20);
    auto loaded = restored(world);
    CHECK(loaded.actor(a)->vehicle == v && loaded.vehicle(v)->occupants[0] == a);
    loaded.grid().set_blocked({4,3},false); OK(loaded.disembark(a));
    CHECK(loaded.vehicle(v)->occupants[0] == no_entity && loaded.actor(a)->vehicle == no_entity);
}
void vehicle_blocks_shots() {
    auto world = fixture();
    const auto a = world.add_actor(Kind::operative,{1.5,2.5});
    const auto enemy = world.add_actor(Kind::security,{7.5,2.5});
    (void)world.add_vehicle({4.5,2.5}); OK(world.equip(a,0,"compact_sidearm",10));
    const auto shot = world.shoot(a,enemy); CHECK(!shot.ok && shot.reason.find("vehicle") != std::string::npos);
    CHECK(world.actor(enemy)->health == 100 && world.actor(a)->inventory[0].ammunition == 10);
}
void acquire_extract_and_reward_once() {
    auto world = fixture(); const auto a = world.add_actor(Kind::operative,{1.5,1.5});
    const auto s = world.add_actor(Kind::specialist,{1.5,2.5});
    world.mission().specialist = s; world.mission().extraction = {1.5,1.5};
    CHECK(!world.extract().ok); OK(world.acquire(a,s)); OK(world.extract()); OK(world.extract());
    CHECK(world.mission().state == MissionState::succeeded && world.credits() == 500);
    auto loaded = restored(world); OK(loaded.extract()); CHECK(loaded.credits() == 500);
}
void extraction_requires_surviving_squad() {
    auto world = fixture(); const auto a = world.add_actor(Kind::operative,{1.5,1.5});
    const auto b = world.add_actor(Kind::operative,{8.5,1.5});
    const auto s = world.add_actor(Kind::specialist,{1.5,2.5});
    world.mission().specialist = s; world.mission().extraction = {1.5,1.5}; OK(world.acquire(a,s));
    CHECK(!world.extract().ok);
    world.actor(b)->health = 0; OK(world.extract()); CHECK(!world.actor(b)->alive());
    auto loaded = restored(world); CHECK(!loaded.actor(b)->alive());
}
void specialist_loss_roster_loss_and_abort() {
    auto world = fixture(); const auto a = world.add_actor(Kind::operative,{1.5,1.5});
    const auto s = world.add_actor(Kind::specialist,{1.5,2.5}); world.mission().specialist = s;
    auto dead_target = world; dead_target.actor(s)->health = 0; dead_target.advance(1.0);
    CHECK(dead_target.mission().state == MissionState::failed);
    CHECK(dead_target.mission().outcome.find("specialist") != std::string::npos);
    auto loaded = restored(dead_target); CHECK(!loaded.actor(s)->alive());
    world.actor(a)->health = 0; world.advance(1.0);
    CHECK(world.mission().state == MissionState::failed); CHECK(world.mission().outcome.find("operatives") != std::string::npos);
    auto aborted = fixture(); aborted.abort(); CHECK(aborted.mission().state == MissionState::aborted);
    CHECK(!aborted.extract().ok);
}
void active_mission_roundtrip_continues() {
    auto world = fixture(); const auto a = world.add_actor(Kind::operative,{1.5,1.5});
    const auto s = world.add_actor(Kind::specialist,{1.5,2.5});
    const auto driver = world.add_actor(Kind::operative,{12.5,4.5});
    const auto v = world.add_vehicle({13.5,4.5});
    world.mission().specialist = s; world.mission().extraction = {8.5,1.5};
    OK(world.acquire(a,s)); OK(world.equip(a,0,"compact_automatic",40)); OK(world.board(driver,v));
    OK(world.issue({a},{OrderKind::move,{7.5,1.5},0}));
    OK(world.drive(driver,{17.5,4.5})); world.grid().set_blocked({20,10},true);
    world.advance(0.17);
    auto loaded = restored(world);
    CHECK(snapshot(loaded) == snapshot(world));
    world.advance(2.0); loaded.advance(2.0);
    CHECK(snapshot(loaded) == snapshot(world));
    CHECK(loaded.actors().size() == 3 && loaded.vehicles().size() == 1);
    CHECK(loaded.actor(s)->follower_of == a && loaded.actor(driver)->vehicle == v);
}
void reject_corrupt_saves_without_mutation() {
    auto world = fixture(); const auto a = world.add_actor(Kind::operative,{1.5,1.5});
    const auto before = snapshot(world);
    std::istringstream truncated(before.substr(0,before.size()/2));
    CHECK(!world.read_snapshot(truncated).ok); CHECK(snapshot(world) == before);
    std::string incompatible = before; incompatible.replace(0,17,"BLACKGLASS_CORE 9");
    std::istringstream old(incompatible); CHECK(!world.read_snapshot(old).ok); CHECK(snapshot(world) == before);
    auto inconsistent = world; inconsistent.actor(a)->vehicle = 999;
    std::istringstream dangling(snapshot(inconsistent)); CHECK(!world.read_snapshot(dangling).ok); CHECK(snapshot(world) == before);
    std::istringstream extra(before + "garbage"); CHECK(!world.read_snapshot(extra).ok); CHECK(snapshot(world) == before);
}
void disk_save_preserves_previous_valid_slot() {
    const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() / ("blackglass-core-" + std::to_string(nonce));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code error; std::filesystem::remove_all(path,error); } } cleanup{directory};
    auto world = fixture(); const auto a = world.add_actor(Kind::operative,{1.5,1.5});
    const auto path = directory / "slot1.bgcore";
    OK(world.save(path)); world.actor(a)->health = 78; OK(world.save(path));
    World loaded(Grid(1,1)); OK(loaded.load(path)); CHECK(loaded.actor(a)->health == 78);
    OK(loaded.load(path.string() + ".previous")); CHECK(loaded.actor(a)->health == 100);
    { std::ofstream broken(path,std::ios::trunc); broken << "corrupted"; }
    const auto before = snapshot(loaded); CHECK(!loaded.load(path).ok); CHECK(snapshot(loaded) == before);
    OK(world.save(path));
    OK(loaded.load(path.string() + ".previous")); CHECK(loaded.actor(a)->health == 100);
    CHECK(!loaded.load(directory / "missing.bgcore").ok);
}
void data_validation_is_transactional() {
    auto world = fixture(); const auto count = world.weapons().size();
    std::istringstream bad("id,range,damage,interval,sound_radius,weight\nbroken,-1,20,0.1,15,2\n");
    CHECK(!read_weapons(bad,world.weapons()).ok); CHECK(world.weapons().size() == count);
    std::istringstream duplicate("id,range,damage,interval,sound_radius,weight\na,10,20,0.1,15,2\na,10,20,0.1,15,2\n");
    CHECK(!read_weapons(duplicate,world.weapons()).ok); CHECK(world.weapons().size() == count);
}
void controller_death_clears_acquisition() {
    auto world = fixture(); const auto a = world.add_actor(Kind::operative,{1.5,1.5});
    (void)world.add_actor(Kind::operative,{8.5,1.5});
    const auto s = world.add_actor(Kind::specialist,{1.5,2.5});
    world.mission().specialist = s; OK(world.acquire(a,s)); world.actor(a)->health = 0; world.advance(0.02);
    CHECK(world.actor(s)->follower_of == no_entity && !world.mission().acquired);
    CHECK(world.mission().state == MissionState::active); CHECK(!world.extract().ok);
}
int main() {
    const std::vector<std::pair<std::string,std::function<void()>>> tests{
        {"four-operative selection and formation movement",select_and_group_move},
        {"individual queues and stop",individual_queue_and_stop},
        {"doorway changes navigation",doorway_changes_navigation},
        {"narrow route and traffic yield",narrow_route_and_traffic_yield},
        {"obstruction preserves ammunition",blocked_shot_preserves_ammunition},
        {"corner collision",corners_block_and_clear_rays},
        {"friendly fire hits actual interceptor",friendly_fire_models_actual_interceptor},
        {"armor and distinct weapon roles",armor_and_weapon_roles},
        {"local security and civilian evidence",security_responds_to_local_evidence},
        {"frame partition and pause",frame_partition_and_pause},
        {"firing cadence across frame partitions",firing_cadence_partition},
        {"transactional equipment transfer",inventory_transfer_is_transactional},
        {"vehicle seats and driver orders",vehicle_seats_and_driver_orders},
        {"blocked exits and wreck persistence",blocked_exit_and_wreck_persist},
        {"vehicles obstruct shooting",vehicle_blocks_shots},
        {"acquire, extract and settle once",acquire_extract_and_reward_once},
        {"surviving squad required at extraction",extraction_requires_surviving_squad},
        {"specialist loss, roster loss and abort",specialist_loss_roster_loss_and_abort},
        {"active mission resumes after load",active_mission_roundtrip_continues},
        {"corrupt saves preserve live state",reject_corrupt_saves_without_mutation},
        {"safe writes retain previous valid slot",disk_save_preserves_previous_valid_slot},
        {"editable weapon data validation",data_validation_is_transactional},
        {"controller death breaks acquisition",controller_death_clears_acquisition}
    };
    int failures = 0;
    for (const auto& test : tests) {
        try { test.second(); std::cout << "PASS: " << test.first << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL: " << test.first << " -- " << e.what() << '\n'; }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures) << '/' << tests.size() << " core contracts passed.\n";
    std::cout << "These are C++ simulation tests. No Unreal gameplay, rendering or packaging has been tested.\n";
    return failures ? 1 : 0;
}
