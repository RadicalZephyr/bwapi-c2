#define offsetof(T,M) __builtin_offsetof(T,M)
template<unsigned long long N> struct SZ;
SZ<sizeof(BWAPI::GameData)> a_GameData;
SZ<sizeof(BWAPI::UnitData)> a_UnitData;
SZ<sizeof(BWAPI::PlayerData)> a_PlayerData;
SZ<sizeof(BWAPI::BulletData)> a_BulletData;
SZ<sizeof(BWAPI::RegionData)> a_RegionData;
SZ<offsetof(BWAPI::GameData, units)> o_units;
SZ<offsetof(BWAPI::GameData, bullets)> o_bullets;
SZ<offsetof(BWAPI::GameData, players)> o_players;
SZ<offsetof(BWAPI::GameData, xUnitSearch)> o_xUnitSearch;
SZ<offsetof(BWAPI::BulletData, angle)> o_bd_angle;
SZ<offsetof(BWAPI::BulletData, isVisible)> o_bd_isVisible;
