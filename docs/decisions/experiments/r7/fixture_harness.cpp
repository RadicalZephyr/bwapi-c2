// R7: drive a real BWAPI client against a synthetic GameData -- no server,
// no shared memory, no pipe, no MPQs. This is the JBWAPI fixture model in C++.
#include <BWAPI.h>
#include <BWAPI/Client/GameImpl.h>
#include <BWAPI/Client.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace BWAPI;

int main() {
  printf("sizeof(GameData) = %zu\n", sizeof(GameData));
  GameData* data = (GameData*)calloc(1, sizeof(GameData));

  // Minimal server-side population -- the fields Server::initializeSharedMemory sets.
  data->client_version = BWAPI::CLIENT_VERSION;
  data->revision       = 4200;
  data->isDebug        = false;
  data->isInGame       = true;
  data->frameCount     = 123;
  data->mapWidth       = 128;
  data->mapHeight      = 128;
  strcpy(data->mapName, "Fixture Map");
  strcpy(data->mapFileName, "(2)Fixture.scx");
  data->hasGUI = true;
  data->hasLatCom = false;
  data->self = 0; data->enemy = 1; data->neutral = 11;
  data->playerCount = 12;
  strcpy(data->players[0].name, "FixtureBot");
  data->players[0].minerals = 350;
  data->players[0].gas      = 75;
  data->players[0].race     = Races::Terran;
  data->players[0].isParticipating = true;

  // One unit: an SCV owned by player 0 at (1000, 2000).
  data->initialUnitCount = 1;
  data->unitArray[0] = 0;
  auto& u = data->units[0];
  u.exists = true; u.player = 0; u.type = UnitTypes::Terran_SCV;
  u.positionX = 1000; u.positionY = 2000;
  u.hitPoints = 60; u.isCompleted = true; u.isVisible[0] = true;
  u.isIdle = true; u.isPowered = true; u.isInterruptible = true; u.order = Orders::PlayerGuard;
  // calloc gives 0, but 0 is a VALID unit index -- BWAPI's "none" is -1.
  // Leaving these zeroed makes the SCV think it is loaded inside unit 0,
  // and canCommand() then fails with Unit_Busy.
  u.transport = u.target = u.orderTarget = u.buildUnit = -1;
  u.addon = u.nydusExit = u.powerUp = u.carrier = u.hatchery = -1;
  u.rallyUnit = -1; u.lastAttackerPlayer = -1;

  // UnitImpl's ctor reads the global BWAPI::BWAPIClient.data, not the pointer
  // handed to GameImpl -- so the fixture has to populate the singleton too.
  BWAPI::BWAPIClient.data = data;

  GameImpl game(data);
  BWAPI::BroodwarPtr = &game;
  game.onMatchStart();

  printf("clientVersion = %d (expected %d)\n", Broodwar->getClientVersion(), BWAPI::CLIENT_VERSION);
  printf("frameCount    = %d\n", Broodwar->getFrameCount());
  printf("map           = %s (%dx%d)\n", Broodwar->mapName().c_str(),
         Broodwar->mapWidth(), Broodwar->mapHeight());
  printf("self          = %s, minerals=%d gas=%d race=%s\n",
         Broodwar->self()->getName().c_str(), Broodwar->self()->minerals(),
         Broodwar->self()->gas(), Broodwar->self()->getRace().c_str());
  printf("allUnits      = %zu\n", Broodwar->getAllUnits().size());
  for (Unit unit : Broodwar->getAllUnits()) {
    printf("  unit %d: %s at (%d,%d) hp=%d/%d  isWorker=%d  mineralPrice=%d\n",
           unit->getID(), unit->getType().c_str(),
           unit->getPosition().x, unit->getPosition().y,
           unit->getHitPoints(), unit->getType().maxHitPoints(),
           (int)unit->getType().isWorker(), unit->getType().mineralPrice());
  }
  // --- write path: does a command land in the outgoing GameData buffer? ---
  Unit scv = Broodwar->getUnit(0);
  printf("  canMove=%d err=%s\n", (int)scv->canMove(), Broodwar->getLastError().c_str());
  printf("  canIssueCommandType(Move)=%d err=%s\n", (int)scv->canIssueCommandType(UnitCommandTypes::Move), Broodwar->getLastError().c_str());
  bool ok = scv->move(Position(1500, 2500));
  printf("move() returned %d; unitCommandCount=%d; lastError=%s\n", (int)ok, data->unitCommandCount, Broodwar->getLastError().c_str());
  printf("  canCommand=%d exists=%d isOwned=%d\n", (int)scv->canCommand(), (int)scv->exists(), (int)(scv->getPlayer()==Broodwar->self()));
  for (int i = 0; i < data->unitCommandCount; ++i) {
    auto& c = data->unitCommands[i];
    printf("  cmd[%d]: type=%s unitIndex=%d x=%d y=%d\n", i,
           UnitCommandType(c.type).c_str(), c.unitIndex, c.x, c.y);
  }
  Broodwar->drawTextScreen(10, 10, "hello fixture");
  printf("shapeCount=%d stringCount=%d\n", data->shapeCount, data->stringCount);

  free(data);
  return 0;
}
