// The static type data with no game at all (plan section 5.8; implementation plan 1.5): the
// constants, the accessors, the container-valued accessors and the bulk tables, every one a pure
// function of an id. No fixture is built and BroodwarPtr stays null throughout; the error channel
// must stay clear, because none of this is allowed to need a connection.
//
// The loops are the point: a few hundred hand-picked values would prove the generator once, and
// a loop over every id proves every wrapper agrees with the C++ it wraps, which is the claim.
#include "doctest.h"

#include "bwapi_c2.h"

#include <BWAPI.h>

#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace BWAPI;

namespace {

std::string name_of(int32_t (*fn)(int32_t, char*, int32_t), int32_t id) {
  char buf[64];
  const int32_t n = fn(id, buf, sizeof buf);
  return std::string(buf, static_cast<size_t>(n < 63 ? n : 63));
}

struct Clear {
  Clear() { bwapi_clear_last_error(); }
  ~Clear() {
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
    bwapi_clear_last_error();
  }
};

// Ids of a class, 0 to Unknown inclusive.
template <class T>
int32_t id_count() {
  return T(-1).getID() + 1;
}

}  // namespace

TEST_CASE("no game: the whole file runs with BroodwarPtr null") {
  Clear c;
  REQUIRE(BroodwarPtr == nullptr);
  CHECK(bwapi_client_is_connected() == 0);
}

TEST_CASE("one anchor per constant family matches upstream") {
  Clear c;
  CHECK(BWAPI_UNIT_NONE == UnitTypes::None.getID());
  CHECK(BWAPI_UNIT_UNKNOWN == UnitTypes::Unknown.getID());
  CHECK(BWAPI_UNIT_TERRAN_MARINE == UnitTypes::Terran_Marine.getID());
  CHECK(BWAPI_UNIT_ALL_UNITS == UnitTypes::AllUnits.getID());
  CHECK(BWAPI_UNIT_MAX == UnitTypes::Enum::MAX);
  CHECK(BWAPI_WEAPON_NONE == WeaponTypes::None.getID());
  CHECK(BWAPI_WEAPON_GAUSS_RIFLE == WeaponTypes::Gauss_Rifle.getID());
  CHECK(BWAPI_TECH_NONE == TechTypes::None.getID());
  CHECK(BWAPI_TECH_STIM_PACKS == TechTypes::Stim_Packs.getID());
  CHECK(BWAPI_UPGRADE_NONE == UpgradeTypes::None.getID());
  CHECK(BWAPI_UPGRADE_U_238_SHELLS == UpgradeTypes::U_238_Shells.getID());
  CHECK(BWAPI_RACE_NONE == Races::None.getID());
  CHECK(BWAPI_RACE_TERRAN == Races::Terran.getID());
  CHECK(BWAPI_RACE_UNKNOWN == Races::Unknown.getID());
  CHECK(BWAPI_UNITSIZE_SMALL == UnitSizeTypes::Small.getID());
  CHECK(BWAPI_DAMAGE_NORMAL == DamageTypes::Normal.getID());
  CHECK(BWAPI_EXPLOSION_NONE == ExplosionTypes::None.getID());
  CHECK(BWAPI_BULLET_NONE == BulletTypes::None.getID());
  CHECK(BWAPI_ORDER_NONE == Orders::None.getID());
  CHECK(BWAPI_ORDER_ATTACK_MOVE == Orders::AttackMove.getID());
  CHECK(BWAPI_PLAYERTYPE_NEUTRAL == PlayerTypes::Neutral.getID());
  CHECK(BWAPI_GAMETYPE_MELEE == GameTypes::Melee.getID());
  CHECK(BWAPI_UNITCOMMAND_ATTACK_MOVE == UnitCommandTypes::Attack_Move.getID());
  CHECK(BWAPI_ERROR_NONE == Errors::None.getID());
  CHECK(BWAPI_ERROR_UNIT_DOES_NOT_EXIST == Errors::Unit_Does_Not_Exist.getID());
  CHECK(BWAPI_COLOR_RED == Colors::Red.getID());
  CHECK(BWAPI_COLOR_BLACK == Colors::Black.getID());
  CHECK(BWAPI_EVENT_MATCH_START == EventType::MatchStart);
  CHECK(BWAPI_EVENT_NONE == EventType::None);
  CHECK(BWAPI_FLAG_COMPLETE_MAP_INFORMATION == Flag::CompleteMapInformation);
  CHECK(BWAPI_COORD_SCREEN == CoordinateType::Screen);
  CHECK(BWAPI_TEXT_BRIGHT_RED == Text::BrightRed);
  CHECK(BWAPI_TEXTSIZE_HUGE == Text::Size::Huge);
  CHECK(BWAPI_KEY_ESCAPE == K_ESCAPE);
  CHECK(BWAPI_MOUSE_LEFT == M_LEFT);
  CHECK(BWAPI_LATENCY_BATTLENET_HIGH == Latency::BattlenetHigh);
  // The position sentinels are BWAPI's own values, unchanged by packing.
  CHECK(BWAPI_POS_X(BWAPI_POSITION_NONE) == Positions::None.x);
  CHECK(BWAPI_POS_Y(BWAPI_POSITION_NONE) == Positions::None.y);
  CHECK(BWAPI_POS_X(BWAPI_TILEPOSITION_UNKNOWN) == TilePositions::Unknown.x);
  CHECK(BWAPI_POS_Y(BWAPI_WALKPOSITION_INVALID) == WalkPositions::Invalid.y);
  // And the ABI's own families are what the header says.
  CHECK(BWAPI_ERR_NONE == 0);
  CHECK(BWAPI_ERR_EXCEPTION == 9);
  CHECK(BWAPI_LOG_ERROR == 2);
}

TEST_CASE("the values the plan names") {
  Clear c;
  CHECK(bwapi_unittype_mineral_price(BWAPI_UNIT_TERRAN_MARINE) == 50);
  CHECK(bwapi_unittype_gas_price(BWAPI_UNIT_TERRAN_MARINE) == 0);
  CHECK(bwapi_unittype_max_hit_points(BWAPI_UNIT_ZERG_ZERGLING) == 35);
  CHECK(bwapi_unittype_max_hit_points(BWAPI_UNIT_TERRAN_MARINE) == 40);
  CHECK(bwapi_unittype_max_shields(BWAPI_UNIT_PROTOSS_ZEALOT) == 60);
  CHECK(bwapi_unittype_supply_required(BWAPI_UNIT_TERRAN_MARINE) == 2);  // doubled, as in BWAPI
  CHECK(bwapi_unittype_is_worker(BWAPI_UNIT_TERRAN_SCV) == 1);
  CHECK(bwapi_unittype_is_worker(BWAPI_UNIT_TERRAN_MARINE) == 0);
  CHECK(bwapi_unittype_is_building(BWAPI_UNIT_TERRAN_BARRACKS) == 1);
  CHECK(bwapi_unittype_get_race(BWAPI_UNIT_ZERG_ZERGLING) == BWAPI_RACE_ZERG);
  CHECK(bwapi_unittype_ground_weapon(BWAPI_UNIT_TERRAN_MARINE) == BWAPI_WEAPON_GAUSS_RIFLE);
  CHECK(bwapi_unittype_top_speed(BWAPI_UNIT_ZERG_ZERGLING) == doctest::Approx(UnitTypes::Zerg_Zergling.topSpeed()));
  CHECK(bwapi_weapontype_damage_amount(BWAPI_WEAPON_GAUSS_RIFLE) == 6);
  CHECK(bwapi_weapontype_max_range(BWAPI_WEAPON_GAUSS_RIFLE) == 128);
  CHECK(bwapi_techtype_mineral_price(BWAPI_TECH_STIM_PACKS) == 100);
  CHECK(bwapi_upgradetype_max_repeats(BWAPI_UPGRADE_TERRAN_INFANTRY_ARMOR) == 3);
  CHECK(bwapi_upgradetype_mineral_price(BWAPI_UPGRADE_TERRAN_INFANTRY_ARMOR, 1) == 100);
  CHECK(bwapi_upgradetype_mineral_price(BWAPI_UPGRADE_TERRAN_INFANTRY_ARMOR, 2) == 175);

  // whatBuilds of a marine is a barracks, one of them.
  int32_t count = -1;
  CHECK(bwapi_unittype_what_builds(BWAPI_UNIT_TERRAN_MARINE, &count) == BWAPI_UNIT_TERRAN_BARRACKS);
  CHECK(count == 1);
  CHECK(bwapi_unittype_what_builds(BWAPI_UNIT_PROTOSS_ARCHON, &count) == BWAPI_UNIT_PROTOSS_HIGH_TEMPLAR);
  CHECK(count == 2);
  CHECK(bwapi_unittype_what_builds(BWAPI_UNIT_TERRAN_MARINE, nullptr) == BWAPI_UNIT_TERRAN_BARRACKS);  // NULL skips the write

  // requiredUnits of a siege tank includes a machine shop.
  int32_t types[8], counts[8];
  const int32_t n = bwapi_unittype_required_units(BWAPI_UNIT_TERRAN_SIEGE_TANK_TANK_MODE, types, counts, 8);
  REQUIRE(n >= 1);
  REQUIRE(n <= 8);
  bool machine_shop = false;
  for (int32_t i = 0; i < n; ++i) {
    if (types[i] == BWAPI_UNIT_TERRAN_MACHINE_SHOP) machine_shop = counts[i] == 1;
    if (i) CHECK(types[i] > types[i - 1]);  // ascending by type id
  }
  CHECK(machine_shop);
  CHECK(bwapi_unittype_required_units(BWAPI_UNIT_TERRAN_SIEGE_TANK_TANK_MODE, nullptr, nullptr, 0) == n);

  // Every race's worker.
  CHECK(bwapi_race_get_worker(BWAPI_RACE_TERRAN) == BWAPI_UNIT_TERRAN_SCV);
  CHECK(bwapi_race_get_worker(BWAPI_RACE_ZERG) == BWAPI_UNIT_ZERG_DRONE);
  CHECK(bwapi_race_get_worker(BWAPI_RACE_PROTOSS) == BWAPI_UNIT_PROTOSS_PROBE);
  CHECK(bwapi_race_get_resource_depot(BWAPI_RACE_TERRAN) == BWAPI_UNIT_TERRAN_COMMAND_CENTER);
  CHECK(bwapi_race_get_supply_provider(BWAPI_RACE_ZERG) == BWAPI_UNIT_ZERG_OVERLORD);

  // Names, through the string convention.
  CHECK(name_of(bwapi_unittype_get_name, BWAPI_UNIT_TERRAN_MARINE) == "Terran_Marine");
  CHECK(name_of(bwapi_unittype_get_name, BWAPI_UNIT_NONE) == "None");
  CHECK(name_of(bwapi_unittype_get_name, 100000) == "Unknown");
  CHECK(name_of(bwapi_race_get_name, BWAPI_RACE_PROTOSS) == "Protoss");
  CHECK(name_of(bwapi_order_get_name, BWAPI_ORDER_ATTACK_MOVE) == "AttackMove");
  CHECK(name_of(bwapi_error_get_name, BWAPI_ERROR_UNIT_DOES_NOT_EXIST) == "Unit_Does_Not_Exist");
  CHECK(bwapi_unittype_get_name(BWAPI_UNIT_TERRAN_MARINE, nullptr, 0) == 13);
  char tiny[4];
  CHECK(bwapi_unittype_get_name(BWAPI_UNIT_TERRAN_MARINE, tiny, sizeof tiny) == 13);
  CHECK(std::string(tiny) == "Ter");

  // isValid: 0 to Unknown inclusive.
  CHECK(bwapi_unittype_is_valid(0) == 1);
  CHECK(bwapi_unittype_is_valid(BWAPI_UNIT_UNKNOWN) == 1);
  CHECK(bwapi_unittype_is_valid(BWAPI_UNIT_UNKNOWN + 1) == 0);
  CHECK(bwapi_unittype_is_valid(-1) == 0);
  CHECK(bwapi_race_is_valid(BWAPI_RACE_UNKNOWN) == 1);
  CHECK(bwapi_race_is_valid(BWAPI_RACE_UNKNOWN + 1) == 0);
}

TEST_CASE("the weapon of every combat unit type is not None") {
  Clear c;
  int combat = 0;
  for (int32_t id = 0; id < id_count<UnitType>(); ++id) {
    const UnitType t(id);
    if (!t.canAttack() || t.isBuilding() || t.isSpell() || t.isHero() || t.isSpecialBuilding()) continue;
    if (t.groundWeapon() == WeaponTypes::None && t.airWeapon() == WeaponTypes::None) continue;
    ++combat;
    const bool armed = bwapi_unittype_ground_weapon(id) != BWAPI_WEAPON_NONE ||
                       bwapi_unittype_air_weapon(id) != BWAPI_WEAPON_NONE;
    CHECK_MESSAGE(armed, t.getName());
  }
  CHECK(combat > 40);
}

TEST_CASE("every UnitType accessor agrees with the C++ it wraps, for every id") {
  Clear c;
  for (int32_t id = -1; id <= id_count<UnitType>(); ++id) {
    const UnitType t(id);
    CHECK(bwapi_unittype_max_hit_points(id) == t.maxHitPoints());
    CHECK(bwapi_unittype_max_shields(id) == t.maxShields());
    CHECK(bwapi_unittype_max_energy(id) == t.maxEnergy());
    CHECK(bwapi_unittype_armor(id) == t.armor());
    CHECK(bwapi_unittype_mineral_price(id) == t.mineralPrice());
    CHECK(bwapi_unittype_gas_price(id) == t.gasPrice());
    CHECK(bwapi_unittype_build_time(id) == t.buildTime());
    CHECK(bwapi_unittype_supply_required(id) == t.supplyRequired());
    CHECK(bwapi_unittype_supply_provided(id) == t.supplyProvided());
    CHECK(bwapi_unittype_space_required(id) == t.spaceRequired());
    CHECK(bwapi_unittype_space_provided(id) == t.spaceProvided());
    CHECK(bwapi_unittype_build_score(id) == t.buildScore());
    CHECK(bwapi_unittype_destroy_score(id) == t.destroyScore());
    CHECK(bwapi_unittype_size(id) == t.size().getID());
    CHECK(bwapi_unittype_tile_width(id) == t.tileWidth());
    CHECK(bwapi_unittype_tile_height(id) == t.tileHeight());
    CHECK(bwapi_unittype_dimension_left(id) == t.dimensionLeft());
    CHECK(bwapi_unittype_dimension_right(id) == t.dimensionRight());
    CHECK(bwapi_unittype_sight_range(id) == t.sightRange());
    CHECK(bwapi_unittype_top_speed(id) == doctest::Approx(t.topSpeed()));
    CHECK(bwapi_unittype_get_race(id) == t.getRace().getID());
    CHECK(bwapi_unittype_ground_weapon(id) == t.groundWeapon().getID());
    CHECK(bwapi_unittype_air_weapon(id) == t.airWeapon().getID());
    CHECK(bwapi_unittype_required_tech(id) == t.requiredTech().getID());
    CHECK(bwapi_unittype_armor_upgrade(id) == t.armorUpgrade().getID());
    CHECK(bwapi_unittype_is_worker(id) == (t.isWorker() ? 1 : 0));
    CHECK(bwapi_unittype_is_building(id) == (t.isBuilding() ? 1 : 0));
    CHECK(bwapi_unittype_is_flyer(id) == (t.isFlyer() ? 1 : 0));
    CHECK(bwapi_unittype_can_attack(id) == (t.canAttack() ? 1 : 0));
    CHECK(bwapi_unittype_is_mineral_field(id) == (t.isMineralField() ? 1 : 0));
    CHECK(bwapi_unittype_tile_size(id) == BWAPI_POS_MAKE(t.tileSize().x, t.tileSize().y));
    CHECK(name_of(bwapi_unittype_get_name, id) == t.getName());
    int32_t count = -1;
    CHECK(bwapi_unittype_what_builds(id, &count) == t.whatBuilds().first.getID());
    CHECK(count == t.whatBuilds().second);
  }
}

TEST_CASE("the other classes' accessors agree with the C++ they wrap, for every id") {
  Clear c;
  for (int32_t id = -1; id <= id_count<WeaponType>(); ++id) {
    const WeaponType w(id);
    CHECK(bwapi_weapontype_damage_amount(id) == w.damageAmount());
    CHECK(bwapi_weapontype_damage_bonus(id) == w.damageBonus());
    CHECK(bwapi_weapontype_damage_cooldown(id) == w.damageCooldown());
    CHECK(bwapi_weapontype_damage_factor(id) == w.damageFactor());
    CHECK(bwapi_weapontype_min_range(id) == w.minRange());
    CHECK(bwapi_weapontype_max_range(id) == w.maxRange());
    CHECK(bwapi_weapontype_get_tech(id) == w.getTech().getID());
    CHECK(bwapi_weapontype_what_uses(id) == w.whatUses().getID());
    CHECK(bwapi_weapontype_damage_type(id) == w.damageType().getID());
    CHECK(bwapi_weapontype_explosion_type(id) == w.explosionType().getID());
    CHECK(bwapi_weapontype_targets_air(id) == (w.targetsAir() ? 1 : 0));
    CHECK(bwapi_weapontype_targets_ground(id) == (w.targetsGround() ? 1 : 0));
    CHECK(name_of(bwapi_weapontype_get_name, id) == w.getName());
  }
  for (int32_t id = -1; id <= id_count<TechType>(); ++id) {
    const TechType t(id);
    CHECK(bwapi_techtype_mineral_price(id) == t.mineralPrice());
    CHECK(bwapi_techtype_gas_price(id) == t.gasPrice());
    CHECK(bwapi_techtype_research_time(id) == t.researchTime());
    CHECK(bwapi_techtype_energy_cost(id) == t.energyCost());
    CHECK(bwapi_techtype_get_race(id) == t.getRace().getID());
    CHECK(bwapi_techtype_what_researches(id) == t.whatResearches().getID());
    CHECK(bwapi_techtype_get_weapon(id) == t.getWeapon().getID());
    CHECK(bwapi_techtype_get_order(id) == t.getOrder().getID());
    CHECK(bwapi_techtype_targets_unit(id) == (t.targetsUnit() ? 1 : 0));
    CHECK(name_of(bwapi_techtype_get_name, id) == t.getName());
  }
  for (int32_t id = -1; id <= id_count<UpgradeType>(); ++id) {
    const UpgradeType u(id);
    for (int32_t level = 1; level <= 3; ++level) {
      CHECK(bwapi_upgradetype_mineral_price(id, level) == u.mineralPrice(level));
      CHECK(bwapi_upgradetype_gas_price(id, level) == u.gasPrice(level));
      CHECK(bwapi_upgradetype_upgrade_time(id, level) == u.upgradeTime(level));
    }
    CHECK(bwapi_upgradetype_max_repeats(id) == u.maxRepeats());
    CHECK(bwapi_upgradetype_get_race(id) == u.getRace().getID());
    CHECK(bwapi_upgradetype_what_upgrades(id) == u.whatUpgrades().getID());
    CHECK(name_of(bwapi_upgradetype_get_name, id) == u.getName());
  }
  for (int32_t id = -1; id <= id_count<Race>(); ++id) {
    const Race r(id);
    CHECK(bwapi_race_get_worker(id) == r.getWorker().getID());
    CHECK(bwapi_race_get_resource_depot(id) == r.getResourceDepot().getID());
    CHECK(bwapi_race_get_refinery(id) == r.getRefinery().getID());
    CHECK(bwapi_race_get_transport(id) == r.getTransport().getID());
    CHECK(bwapi_race_get_supply_provider(id) == r.getSupplyProvider().getID());
    CHECK(name_of(bwapi_race_get_name, id) == r.getName());
  }
  for (int32_t id = -1; id <= id_count<Order>(); ++id) CHECK(name_of(bwapi_order_get_name, id) == Order(id).getName());
  for (int32_t id = -1; id <= id_count<Error>(); ++id) CHECK(name_of(bwapi_error_get_name, id) == Error(id).getName());
  for (int32_t id = -1; id <= id_count<UnitCommandType>(); ++id)
    CHECK(name_of(bwapi_unitcommandtype_get_name, id) == UnitCommandType(id).getName());
  for (int32_t id = -1; id <= id_count<PlayerType>(); ++id) {
    CHECK(bwapi_playertype_is_lobby_type(id) == (PlayerType(id).isLobbyType() ? 1 : 0));
    CHECK(bwapi_playertype_is_game_type(id) == (PlayerType(id).isGameType() ? 1 : 0));
  }
  for (int32_t id = 0; id < 256; ++id) {
    CHECK(bwapi_color_red(id) == Color(id).red());
    CHECK(bwapi_color_green(id) == Color(id).green());
    CHECK(bwapi_color_blue(id) == Color(id).blue());
  }
}

TEST_CASE("the container-valued accessors follow the collection convention") {
  Clear c;
  int32_t ids[64];
  SUBCASE("abilities, upgrades, buildsWhat, whatUses") {
    int32_t n = bwapi_unittype_abilities(BWAPI_UNIT_TERRAN_GHOST, ids, 64);
    std::set<int32_t> got(ids, ids + n);
    CHECK(got.count(BWAPI_TECH_LOCKDOWN) == 1);
    CHECK(got.count(BWAPI_TECH_PERSONNEL_CLOAKING) == 1);
    CHECK(got.count(BWAPI_TECH_NUCLEAR_STRIKE) == 1);
    CHECK(got.size() == UnitTypes::Terran_Ghost.abilities().size());
    for (int32_t i = 1; i < n; ++i) CHECK(ids[i] > ids[i - 1]);

    n = bwapi_unittype_upgrades(BWAPI_UNIT_TERRAN_MARINE, ids, 64);
    got.clear();
    got.insert(ids, ids + n);
    CHECK(got.count(BWAPI_UPGRADE_TERRAN_INFANTRY_ARMOR) == 1);
    CHECK(got.count(BWAPI_UPGRADE_TERRAN_INFANTRY_WEAPONS) == 1);
    CHECK(got.count(BWAPI_UPGRADE_U_238_SHELLS) == 1);

    n = bwapi_unittype_builds_what(BWAPI_UNIT_TERRAN_BARRACKS, ids, 64);
    got.clear();
    got.insert(ids, ids + n);
    CHECK(got.count(BWAPI_UNIT_TERRAN_MARINE) == 1);
    CHECK(got.count(BWAPI_UNIT_TERRAN_FIREBAT) == 1);
    CHECK(got.count(BWAPI_UNIT_TERRAN_MEDIC) == 1);
    CHECK(got.count(BWAPI_UNIT_TERRAN_GHOST) == 1);
    CHECK(got.count(BWAPI_UNIT_TERRAN_SCV) == 0);

    n = bwapi_techtype_what_uses(BWAPI_TECH_STIM_PACKS, ids, 64);
    got.clear();
    got.insert(ids, ids + n);
    CHECK(got.count(BWAPI_UNIT_TERRAN_MARINE) == 1);
    CHECK(got.count(BWAPI_UNIT_TERRAN_FIREBAT) == 1);
    CHECK(n == static_cast<int32_t>(TechTypes::Stim_Packs.whatUses().size()));

    n = bwapi_upgradetype_what_uses(BWAPI_UPGRADE_ZERG_MELEE_ATTACKS, ids, 64);
    got.clear();
    got.insert(ids, ids + n);
    CHECK(got.count(BWAPI_UNIT_ZERG_ZERGLING) == 1);
    CHECK(got.count(BWAPI_UNIT_ZERG_ULTRALISK) == 1);
  }
  SUBCASE("a short buffer, the size query, and the empty set") {
    const int32_t total = bwapi_unittype_builds_what(BWAPI_UNIT_TERRAN_BARRACKS, nullptr, 0);
    CHECK(total == static_cast<int32_t>(UnitTypes::Terran_Barracks.buildsWhat().size()));
    ids[0] = ids[1] = -7;
    CHECK(bwapi_unittype_builds_what(BWAPI_UNIT_TERRAN_BARRACKS, ids, 1) == total);
    CHECK(ids[0] == BWAPI_UNIT_TERRAN_MARINE);  // the lowest id first
    CHECK(ids[1] == -7);
    CHECK(bwapi_unittype_abilities(BWAPI_UNIT_TERRAN_SUPPLY_DEPOT, ids, 64) == 0);
    CHECK(bwapi_unittype_required_units(BWAPI_UNIT_TERRAN_SCV, nullptr, nullptr, 0) ==
          static_cast<int32_t>(UnitTypes::Terran_SCV.requiredUnits().size()));
  }
  SUBCASE("every id's containers agree with the C++ in size and membership") {
    for (int32_t id = 0; id < id_count<UnitType>(); ++id) {
      const UnitType t(id);
      const int32_t n = bwapi_unittype_abilities(id, ids, 64);
      CHECK(n == static_cast<int32_t>(t.abilities().size()));
      for (int32_t i = 0; i < n && i < 64; ++i) CHECK(t.abilities().count(TechType(ids[i])) == 1);
      int32_t types[16], counts[16];
      const int32_t r = bwapi_unittype_required_units(id, types, counts, 16);
      REQUIRE(r <= 16);
      CHECK(r == static_cast<int32_t>(t.requiredUnits().size()));
      for (int32_t i = 0; i < r; ++i) {
        const auto it = t.requiredUnits().find(UnitType(types[i]));
        REQUIRE(it != t.requiredUnits().end());
        CHECK(it->second == counts[i]);
      }
    }
  }
  SUBCASE("a bad buffer latches") {
    CHECK(bwapi_unittype_abilities(BWAPI_UNIT_TERRAN_GHOST, nullptr, 4) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    bwapi_clear_last_error();
    CHECK(bwapi_unittype_required_units(BWAPI_UNIT_TERRAN_GHOST, nullptr, ids, 4) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    bwapi_clear_last_error();
  }
}

TEST_CASE("every table row equals the accessor for the same id") {
  Clear c;
  SUBCASE("UnitType") {
    std::vector<bwapi_unittype_row> rows(static_cast<size_t>(id_count<UnitType>()));
    rows[0].size = sizeof(bwapi_unittype_row);
    const int32_t total = bwapi_unittype_table(rows.data(), static_cast<int32_t>(rows.size()));
    REQUIRE(total == id_count<UnitType>());
    for (int32_t id = 0; id < total; ++id) {
      const auto& r = rows[static_cast<size_t>(id)];
      CHECK(r.size == sizeof(bwapi_unittype_row));
      CHECK(r.id == id);
      CHECK(r.max_hit_points == bwapi_unittype_max_hit_points(id));
      CHECK(r.max_shields == bwapi_unittype_max_shields(id));
      CHECK(r.mineral_price == bwapi_unittype_mineral_price(id));
      CHECK(r.gas_price == bwapi_unittype_gas_price(id));
      CHECK(r.build_time == bwapi_unittype_build_time(id));
      CHECK(r.supply_required == bwapi_unittype_supply_required(id));
      CHECK(r.top_speed == bwapi_unittype_top_speed(id));
      CHECK(r.get_race == bwapi_unittype_get_race(id));
      CHECK(r.ground_weapon == bwapi_unittype_ground_weapon(id));
      CHECK(r.air_weapon == bwapi_unittype_air_weapon(id));
      CHECK(r.size_type == bwapi_unittype_size(id));
      CHECK(r.is_worker == bwapi_unittype_is_worker(id));
      CHECK(r.is_building == bwapi_unittype_is_building(id));
      CHECK(r.is_flyer == bwapi_unittype_is_flyer(id));
      CHECK(r.can_attack == bwapi_unittype_can_attack(id));
      CHECK(r.is_mineral_field == bwapi_unittype_is_mineral_field(id));
      CHECK(r.sight_range == bwapi_unittype_sight_range(id));
      CHECK(r.armor_upgrade == bwapi_unittype_armor_upgrade(id));
    }
  }
  SUBCASE("WeaponType, TechType, UpgradeType") {
    std::vector<bwapi_weapontype_row> w(static_cast<size_t>(id_count<WeaponType>()));
    w[0].size = sizeof(bwapi_weapontype_row);
    REQUIRE(bwapi_weapontype_table(w.data(), static_cast<int32_t>(w.size())) == id_count<WeaponType>());
    for (int32_t id = 0; id < id_count<WeaponType>(); ++id) {
      const auto& r = w[static_cast<size_t>(id)];
      CHECK(r.id == id);
      CHECK(r.damage_amount == bwapi_weapontype_damage_amount(id));
      CHECK(r.damage_cooldown == bwapi_weapontype_damage_cooldown(id));
      CHECK(r.max_range == bwapi_weapontype_max_range(id));
      CHECK(r.what_uses == bwapi_weapontype_what_uses(id));
      CHECK(r.damage_type == bwapi_weapontype_damage_type(id));
      CHECK(r.targets_air == bwapi_weapontype_targets_air(id));
    }
    std::vector<bwapi_techtype_row> t(static_cast<size_t>(id_count<TechType>()));
    t[0].size = sizeof(bwapi_techtype_row);
    REQUIRE(bwapi_techtype_table(t.data(), static_cast<int32_t>(t.size())) == id_count<TechType>());
    for (int32_t id = 0; id < id_count<TechType>(); ++id) {
      const auto& r = t[static_cast<size_t>(id)];
      CHECK(r.id == id);
      CHECK(r.mineral_price == bwapi_techtype_mineral_price(id));
      CHECK(r.gas_price == bwapi_techtype_gas_price(id));
      CHECK(r.research_time == bwapi_techtype_research_time(id));
      CHECK(r.energy_cost == bwapi_techtype_energy_cost(id));
      CHECK(r.get_race == bwapi_techtype_get_race(id));
      CHECK(r.what_researches == bwapi_techtype_what_researches(id));
      CHECK(r.get_weapon == bwapi_techtype_get_weapon(id));
      CHECK(r.targets_unit == bwapi_techtype_targets_unit(id));
      CHECK(r.targets_position == bwapi_techtype_targets_position(id));
      CHECK(r.get_order == bwapi_techtype_get_order(id));
      CHECK(r.required_unit == bwapi_techtype_required_unit(id));
    }
    std::vector<bwapi_upgradetype_row> u(static_cast<size_t>(id_count<UpgradeType>()));
    u[0].size = sizeof(bwapi_upgradetype_row);
    REQUIRE(bwapi_upgradetype_table(u.data(), static_cast<int32_t>(u.size())) == id_count<UpgradeType>());
    for (int32_t id = 0; id < id_count<UpgradeType>(); ++id) {
      const auto& r = u[static_cast<size_t>(id)];
      CHECK(r.id == id);
      CHECK(r.get_race == bwapi_upgradetype_get_race(id));
      CHECK(r.max_repeats == bwapi_upgradetype_max_repeats(id));
      CHECK(r.what_upgrades == bwapi_upgradetype_what_upgrades(id));
    }
  }
  SUBCASE("Race, PlayerType, Color: every field") {
    std::vector<bwapi_race_row> races(static_cast<size_t>(id_count<Race>()));
    races[0].size = sizeof(bwapi_race_row);
    REQUIRE(bwapi_race_table(races.data(), static_cast<int32_t>(races.size())) == id_count<Race>());
    for (int32_t id = 0; id < id_count<Race>(); ++id) {
      const auto& r = races[static_cast<size_t>(id)];
      CHECK(r.id == id);
      CHECK(r.get_worker == bwapi_race_get_worker(id));
      CHECK(r.get_resource_depot == bwapi_race_get_resource_depot(id));
      CHECK(r.get_refinery == bwapi_race_get_refinery(id));
      CHECK(r.get_transport == bwapi_race_get_transport(id));
      CHECK(r.get_supply_provider == bwapi_race_get_supply_provider(id));
    }
    std::vector<bwapi_playertype_row> pt(static_cast<size_t>(id_count<PlayerType>()));
    pt[0].size = sizeof(bwapi_playertype_row);
    REQUIRE(bwapi_playertype_table(pt.data(), static_cast<int32_t>(pt.size())) == id_count<PlayerType>());
    for (int32_t id = 0; id < id_count<PlayerType>(); ++id) {
      CHECK(pt[static_cast<size_t>(id)].is_lobby_type == bwapi_playertype_is_lobby_type(id));
      CHECK(pt[static_cast<size_t>(id)].is_game_type == bwapi_playertype_is_game_type(id));
    }
    std::vector<bwapi_color_row> colors(256);
    colors[0].size = sizeof(bwapi_color_row);
    REQUIRE(bwapi_color_table(colors.data(), 256) == 256);
    for (int32_t id = 0; id < 256; ++id) {
      CHECK(colors[static_cast<size_t>(id)].red == bwapi_color_red(id));
      CHECK(colors[static_cast<size_t>(id)].green == bwapi_color_green(id));
      CHECK(colors[static_cast<size_t>(id)].blue == bwapi_color_blue(id));
    }
  }
}

TEST_CASE("the stride rule on a table") {
  Clear c;
  const int32_t total = bwapi_race_table(nullptr, 0);
  REQUIRE(total == id_count<Race>());

  SUBCASE("a shorter stride fills only the prefix the caller has room for") {
    // Two int32_t per row: size and id. A consumer compiled against an older header.
    struct OldRow { int32_t size, id; };
    std::vector<OldRow> old(static_cast<size_t>(total), OldRow{0, -5});
    old[0].size = sizeof(OldRow);
    CHECK(bwapi_race_table(reinterpret_cast<bwapi_race_row*>(old.data()), total) == total);
    for (int32_t i = 0; i < total; ++i) {
      CHECK(old[static_cast<size_t>(i)].size == sizeof(OldRow));
      CHECK(old[static_cast<size_t>(i)].id == i);
    }
    CHECK_FALSE(BWAPI_HAS_FIELD(bwapi_race_row, get_worker, old[0].size));
    CHECK(BWAPI_HAS_FIELD(bwapi_race_row, id, old[0].size));
  }
  SUBCASE("a longer stride zero-fills the remainder and reports the bytes filled") {
    // A consumer compiled against a newer header, with a field this DLL does not know.
    struct NewRow { bwapi_race_row known; int32_t future; };
    std::vector<NewRow> rows(static_cast<size_t>(total));
    for (auto& r : rows) r.future = 0x5a5a5a5a;
    rows[0].known.size = sizeof(NewRow);
    CHECK(bwapi_race_table(reinterpret_cast<bwapi_race_row*>(rows.data()), total) == total);
    for (int32_t i = 0; i < total; ++i) {
      CHECK(rows[static_cast<size_t>(i)].known.size == sizeof(bwapi_race_row));
      CHECK(rows[static_cast<size_t>(i)].known.id == i);
      CHECK(rows[static_cast<size_t>(i)].future == 0);
      CHECK(rows[static_cast<size_t>(i)].known.get_worker == bwapi_race_get_worker(i));
    }
    CHECK(BWAPI_HAS_FIELD(bwapi_race_row, get_supply_provider, rows[0].known.size));
    CHECK_FALSE(BWAPI_HAS_FIELD(NewRow, future, rows[0].known.size));
  }
  SUBCASE("a short cap holds the first cap rows and reports the total") {
    bwapi_race_row two[2];
    two[0].size = sizeof(bwapi_race_row);
    two[1].id = -9;
    CHECK(bwapi_race_table(two, 1) == total);
    CHECK(two[0].id == 0);
    CHECK(two[1].id == -9);
  }
  SUBCASE("a stride too small for size itself is a bad buffer") {
    bwapi_race_row one;
    one.size = 0;
    CHECK(bwapi_race_table(&one, 1) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    bwapi_clear_last_error();
    CHECK(bwapi_race_table(nullptr, 3) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
    bwapi_clear_last_error();
  }
}

TEST_CASE("the flat requiredUnits table is every type's requirements, sorted") {
  Clear c;
  const int32_t total = bwapi_unittype_required_units_table(nullptr, 0);
  REQUIRE(total > 100);
  std::vector<bwapi_required_unit> rows(static_cast<size_t>(total));
  rows[0].size = sizeof(bwapi_required_unit);
  REQUIRE(bwapi_unittype_required_units_table(rows.data(), total) == total);

  int32_t expected = 0;
  for (int32_t id = 0; id < id_count<UnitType>(); ++id) expected += static_cast<int32_t>(UnitType(id).requiredUnits().size());
  CHECK(total == expected);

  bool tank_needs_shop = false;
  for (int32_t i = 0; i < total; ++i) {
    const auto& r = rows[static_cast<size_t>(i)];
    CHECK(r.size == sizeof(bwapi_required_unit));
    if (i) {
      const auto& p = rows[static_cast<size_t>(i - 1)];
      CHECK((r.type > p.type || (r.type == p.type && r.required_type > p.required_type)));
    }
    const auto& req = UnitType(r.type).requiredUnits();
    const auto it = req.find(UnitType(r.required_type));
    REQUIRE(it != req.end());
    CHECK(it->second == r.count);
    if (r.type == BWAPI_UNIT_TERRAN_SIEGE_TANK_TANK_MODE && r.required_type == BWAPI_UNIT_TERRAN_MACHINE_SHOP)
      tank_needs_shop = r.count == 1;
  }
  CHECK(tank_needs_shop);
}
