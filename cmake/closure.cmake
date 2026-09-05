# The client-mode link closure of BWAPI plus BWEM, as one object library (plan section 10.1,
# derived empirically in R6 and R11.4). Every source file is named: a file upstream adds is a
# link error here, on purpose, and gets a decision at the next pin bump (section 10.3).
#
# Both trees nest their sources one directory down (docs/pins.md); every path below is relative
# to the inner roots.

set(BWAPI_C2_BWAPI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bwapi/bwapi")
set(BWAPI_C2_BWEM_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bwem/BWEM")

# BWAPILIB/Source/*.cpp except Streams.cpp (the sole definition of bwout/bwerr/out/err, which
# section 5.9 excludes and nothing in the closure references) and BroodwarOutputDevice.cpp
# (entirely inside #if 0).
set(_bwapi_c2_bwapilib_sources
  AIModule.cpp
  BWAPI.cpp
  BulletType.cpp
  Color.cpp
  DamageType.cpp
  Error.cpp
  Event.cpp
  ExplosionType.cpp
  Filters.cpp
  Forceset.cpp
  Game.cpp
  GameType.cpp
  Order.cpp
  Player.cpp
  PlayerType.cpp
  Playerset.cpp
  Position.cpp
  Race.cpp
  Region.cpp
  Regionset.cpp
  TechType.cpp
  Unit.cpp
  UnitCommandType.cpp
  UnitSizeType.cpp
  UnitType.cpp
  Unitset.cpp
  UpgradeType.cpp
  WeaponType.cpp
)
list(TRANSFORM _bwapi_c2_bwapilib_sources PREPEND "${BWAPI_C2_BWAPI_ROOT}/BWAPILIB/Source/")

# Templates.h and the *Shared.cpp implementations that both the injected DLL and the client use.
set(_bwapi_c2_shared_sources
  BulletShared.cpp
  ForceShared.cpp
  GameShared.cpp
  PlayerShared.cpp
  RegionShared.cpp
  UnitShared.cpp
)
list(TRANSFORM _bwapi_c2_shared_sources PREPEND "${BWAPI_C2_BWAPI_ROOT}/Shared/")

# The client: transport, and the client-side GameImpl/UnitImpl/... over GameData.
set(_bwapi_c2_client_sources
  BulletImpl.cpp
  Client.cpp
  ForceImpl.cpp
  GameImpl.cpp
  PlayerImpl.cpp
  RegionImpl.cpp
  UnitImpl.cpp
)
list(TRANSFORM _bwapi_c2_client_sources PREPEND "${BWAPI_C2_BWAPI_ROOT}/BWAPIClient/Source/")

# BWEM: fourteen translation units, needing ten BWAPI symbols that are all above (R11.4).
set(_bwapi_c2_bwem_sources
  area.cpp
  base.cpp
  bwapiExt.cpp
  bwem.cpp
  cp.cpp
  graph.cpp
  gridMap.cpp
  map.cpp
  mapDrawer.cpp
  mapImpl.cpp
  mapPrinter.cpp
  neutral.cpp
  tiles.cpp
  utils.cpp
)
list(TRANSFORM _bwapi_c2_bwem_sources PREPEND "${BWAPI_C2_BWEM_ROOT}/src/")

add_library(bwapi_c2_closure OBJECT
  "${BWAPI_C2_BWAPI_ROOT}/BWAPILIB/UnitCommand.cpp"
  ${_bwapi_c2_bwapilib_sources}
  ${_bwapi_c2_shared_sources}
  ${_bwapi_c2_client_sources}
  ${_bwapi_c2_bwem_sources}
)

# The object library ends up inside a shared library, and tests link it directly to reach
# GameImpl without crossing the C boundary.
set_target_properties(bwapi_c2_closure PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The include list is load-bearing (section 10.1): Shared/*.cpp include their Impl headers
# unqualified and resolve them against include/BWAPI/Client, which is the one that is easy to
# miss. svnrev.h lives in include/ on our fork (section 10.3), so nothing is generated here.
target_include_directories(bwapi_c2_closure PUBLIC
  "${BWAPI_C2_BWAPI_ROOT}/include"
  "${BWAPI_C2_BWAPI_ROOT}/include/BWAPI/Client"
  "${BWAPI_C2_BWAPI_ROOT}/Shared"
  "${BWAPI_C2_BWAPI_ROOT}/BWAPIClient/Source"
  "${BWAPI_C2_BWEM_ROOT}/include"
)

target_compile_definitions(bwapi_c2_closure PUBLIC NOMINMAX=1)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # CommandTemp.h:34 relies on MSVC's two-phase lookup semantics. Public, because anything that
  # includes BWAPIClient/Source/Command.h needs it too.
  target_compile_options(bwapi_c2_closure PUBLIC -fdelayed-template-parsing)
endif()

if(MSVC)
  target_compile_options(bwapi_c2_closure PRIVATE /W0)
else()
  # Upstream's code, upstream's warnings; ours are enabled on our own targets.
  target_compile_options(bwapi_c2_closure PRIVATE -w -fvisibility=hidden)
endif()

if(BWAPI_CUSTOM_COMPILE_FLAGS)
  separate_arguments(_bwapi_c2_custom_flags NATIVE_COMMAND "${BWAPI_CUSTOM_COMPILE_FLAGS}")
  target_compile_options(bwapi_c2_closure PRIVATE ${_bwapi_c2_custom_flags})
endif()

if(NOT WIN32)
  # Client.cpp is the only file that touches Win32: seven imports, all transport (Appendix B).
  # The shim declares exactly that surface and the stub defines it, so the closure links and the
  # whole test suite runs on Linux (R6, section 11). Test-only scaffolding, hence under tests/.
  target_include_directories(bwapi_c2_closure PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/shim")
  target_sources(bwapi_c2_closure PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/win32stub.cpp")
endif()
