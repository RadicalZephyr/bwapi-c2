# R2 appendix: frequency-ranked BWAPI entry points

Generated from 7 corpora (ZZZKBot, UAlbertaBot, Steamhammer, RazeAndPlunder,
OpprimoBot, ExampleAIModule, ExampleAIClient). Conservative count: a name is
dropped for a given bot if that bot also defines a method of the same name.

`v1` marks the proposed cut line: used by >= 2 corpora, or >= 10 call sites.

| # | entry point | call sites | cum % | corpora | BWAPI class(es) | v1 |
|---|---|---|---|---|---|---|
| 1 | `getType` | 549 | 6.5% | 5 | Bullet, BulletType, DamageType, Error, Event, ExplosionType, GameType, Order, Player, PlayerType, Race, TechType, Unit, UnitCommand, UnitCommandType, UnitSizeType, UnitType, UpgradeType, WeaponType | yes |
| 2 | `self` | 516 | 12.7% | 6 | Game | yes |
| 3 | `getFrameCount` | 287 | 16.1% | 7 | Game | yes |
| 4 | `c_str` | 283 | 19.4% | 7 | BulletType, DamageType, Error, ExplosionType, GameType, Order, PlayerType, Race, TechType, UnitCommandType, UnitSizeType, UnitType, UpgradeType, WeaponType | yes |
| 5 | `getPosition` | 253 | 22.5% | 5 | Bullet, Event, Unit, Unitset | yes |
| 6 | `drawTextScreen` | 230 | 25.2% | 7 | Game | yes |
| 7 | `getID` | 221 | 27.8% | 4 | Bullet, BulletType, DamageType, Error, ExplosionType, Force, GameType, Order, Player, PlayerType, Race, Region, TechType, Unit, UnitCommandType, UnitSizeType, UnitType, UpgradeType, WeaponType | yes |
| 8 | `getTilePosition` | 188 | 30.1% | 5 | Unit | yes |
| 9 | `getRace` | 184 | 32.2% | 6 | Player, TechType, UnitType, UpgradeType | yes |
| 10 | `enemy` | 166 | 34.2% | 6 | Game | yes |
| 11 | `drawTextMap` | 145 | 35.9% | 6 | Game | yes |
| 12 | `exists` | 141 | 37.6% | 5 | Bullet, Unit | yes |
| 13 | `groundWeapon` | 132 | 39.2% | 5 | UnitType | yes |
| 14 | `getPlayer` | 130 | 40.7% | 6 | Bullet, Event, Game, Unit | yes |
| 15 | `isFlyer` | 96 | 41.9% | 4 | UnitType | yes |
| 16 | `drawLineMap` | 95 | 43.0% | 5 | Game | yes |
| 17 | `getHitPoints` | 95 | 44.1% | 5 | Unit | yes |
| 18 | `drawBoxMap` | 91 | 45.2% | 5 | Game | yes |
| 19 | `airWeapon` | 83 | 46.2% | 5 | UnitType | yes |
| 20 | `getName` | 81 | 47.2% | 4 | BulletType, DamageType, Error, ExplosionType, Force, GameType, Order, Player, PlayerType, Race, TechType, UnitCommandType, UnitSizeType, UnitType, UpgradeType, WeaponType | yes |
| 21 | `drawCircleMap` | 78 | 48.1% | 5 | Game | yes |
| 22 | `getStartLocation` | 75 | 49.0% | 5 | Player | yes |
| 23 | `isBeingConstructed` | 74 | 49.9% | 5 | Unit | yes |
| 24 | `maxRange` | 73 | 50.7% | 4 | WeaponType | yes |
| 25 | `isUpgrading` | 72 | 51.6% | 4 | Player, Unit | yes |
| 26 | `getDistance` | 71 | 52.4% | 3 | Position, Region, Unit | yes |
| 27 | `hasResearched` | 71 | 53.3% | 4 | Player | yes |
| 28 | `isWorker` | 70 | 54.1% | 5 | UnitType | yes |
| 29 | `getUnits` | 69 | 54.9% | 5 | Playerset, Region, Regionset | yes |
| 30 | `isFlying` | 69 | 55.8% | 3 | Unit | yes |
| 31 | `printf` | 65 | 56.5% | 2 | Game | yes |
| 32 | `getEnergy` | 64 | 57.3% | 4 | Unit | yes |
| 33 | `getUpgradeLevel` | 63 | 58.0% | 5 | Player | yes |
| 34 | `mapHeight` | 62 | 58.8% | 6 | Game | yes |
| 35 | `mapWidth` | 62 | 59.5% | 6 | Game | yes |
| 36 | `tileHeight` | 59 | 60.2% | 5 | UnitType | yes |
| 37 | `isIdle` | 57 | 60.9% | 6 | Unit | yes |
| 38 | `tileWidth` | 57 | 61.6% | 5 | UnitType | yes |
| 39 | `getClosestUnit` | 55 | 62.2% | 3 | Game, Unit, Unitset | yes |
| 40 | `isBurrowed` | 55 | 62.9% | 3 | Unit | yes |
| 41 | `canAttack` | 52 | 63.5% | 4 | Unit, UnitType | yes |
| 42 | `getLastCommand` | 46 | 64.0% | 3 | Unit | yes |
| 43 | `isResourceDepot` | 46 | 64.6% | 7 | UnitType | yes |
| 44 | `getApproxDistance` | 45 | 65.1% | 3 | Position | yes |
| 45 | `useTech` | 45 | 65.7% | 4 | Unit, UnitCommand, Unitset | yes |
| 46 | `sightRange` | 41 | 66.2% | 3 | Player, UnitType | yes |
| 47 | `rightClick` | 40 | 66.6% | 6 | Unit, UnitCommand, Unitset | yes |
| 48 | `size` | 40 | 67.1% | 2 | UnitType | yes |
| 49 | `getShields` | 38 | 67.6% | 5 | Unit | yes |
| 50 | `isDetector` | 38 | 68.0% | 4 | UnitType | yes |
| 51 | `minerals` | 37 | 68.4% | 5 | Player | yes |
| 52 | `isCompleted` | 36 | 68.9% | 5 | Unit | yes |
| 53 | `canMake` | 35 | 69.3% | 5 | Game | yes |
| 54 | `maxHitPoints` | 35 | 69.7% | 5 | UnitType | yes |
| 55 | `getGroundWeaponCooldown` | 34 | 70.1% | 5 | Unit | yes |
| 56 | `getUnitType` | 34 | 70.5% | 1 | UnitCommand | yes |
| 57 | `isBuilding` | 32 | 70.9% | 4 | UnitType | yes |
| 58 | `supplyUsed` | 32 | 71.3% | 4 | Player | yes |
| 59 | `train` | 32 | 71.7% | 7 | Unit, UnitCommand, Unitset | yes |
| 60 | `getTarget` | 31 | 72.0% | 3 | Bullet, Unit, UnitCommand | yes |
| 61 | `isMoving` | 31 | 72.4% | 5 | Unit | yes |
| 62 | `isResearching` | 30 | 72.8% | 3 | Player, Unit | yes |
| 63 | `targetsAir` | 30 | 73.1% | 2 | WeaponType | yes |
| 64 | `deadUnitCount` | 29 | 73.5% | 3 | Player | yes |
| 65 | `isReplay` | 29 | 73.8% | 5 | Game | yes |
| 66 | `mapFileName` | 28 | 74.1% | 5 | Game | yes |
| 67 | `isConstructing` | 27 | 74.5% | 5 | Unit | yes |
| 68 | `isLoaded` | 27 | 74.8% | 4 | Unit | yes |
| 69 | `mineralPrice` | 26 | 75.1% | 3 | TechType, UnitType, UpgradeType | yes |
| 70 | `targetsGround` | 26 | 75.4% | 2 | WeaponType | yes |
| 71 | `weaponMaxRange` | 26 | 75.7% | 2 | Player | yes |
| 72 | `drawBoxScreen` | 25 | 76.0% | 4 | Game | yes |
| 73 | `getLastCommandFrame` | 25 | 76.3% | 3 | Unit | yes |
| 74 | `getBuildType` | 24 | 76.6% | 5 | Unit | yes |
| 75 | `morph` | 24 | 76.9% | 5 | Unit, UnitCommand, Unitset | yes |
| 76 | `setClientInfo` | 24 | 77.2% | 1 | Unitset | yes |
| 77 | `topSpeed` | 24 | 77.4% | 2 | Player, UnitType | yes |
| 78 | `dimensionDown` | 23 | 77.7% | 5 | UnitType | yes |
| 79 | `dimensionLeft` | 23 | 78.0% | 5 | UnitType | yes |
| 80 | `dimensionRight` | 23 | 78.3% | 5 | UnitType | yes |
| 81 | `drawLineScreen` | 23 | 78.5% | 2 | Game | yes |
| 82 | `sendText` | 23 | 78.8% | 6 | Game | yes |
| 83 | `dimensionUp` | 22 | 79.1% | 5 | UnitType | yes |
| 84 | `getInitialTilePosition` | 22 | 79.3% | 2 | Unit | yes |
| 85 | `getRemainingBuildTime` | 22 | 79.6% | 5 | Unit | yes |
| 86 | `getWorker` | 22 | 79.9% | 7 | Race | yes |
| 87 | `isAttackFrame` | 22 | 80.1% | 3 | Unit | yes |
| 88 | `isDetected` | 22 | 80.4% | 3 | Unit | yes |
| 89 | `isResourceContainer` | 22 | 80.6% | 4 | UnitType | yes |
| 90 | `isLifted` | 21 | 80.9% | 5 | Unit | yes |
| 91 | `isRepairing` | 21 | 81.1% | 5 | Unit | yes |
| 92 | `isSieged` | 21 | 81.4% | 4 | Unit | yes |
| 93 | `isAddon` | 20 | 81.6% | 3 | UnitType | yes |
| 94 | `move` | 20 | 81.9% | 4 | Unit, UnitCommand, Unitset | yes |
| 95 | `buildTime` | 19 | 82.1% | 5 | UnitType | yes |
| 96 | `getInitialPosition` | 19 | 82.3% | 2 | Unit | yes |
| 97 | `getUnit` | 19 | 82.5% | 2 | Event, Game, UnitCommand | yes |
| 98 | `isCarryingMinerals` | 19 | 82.8% | 3 | Unit | yes |
| 99 | `isCloaked` | 19 | 83.0% | 3 | Unit | yes |
| 100 | `maxShields` | 19 | 83.2% | 3 | UnitType | yes |
| 101 | `supplyTotal` | 19 | 83.4% | 5 | Player | yes |
| 102 | `flush` | 17 | 83.6% | 4 | Game | yes |
| 103 | `gas` | 17 | 83.8% | 5 | Player | yes |
| 104 | `getAirWeaponCooldown` | 17 | 84.1% | 5 | Unit | yes |
| 105 | `getResources` | 17 | 84.3% | 5 | Unit | yes |
| 106 | `attack` | 16 | 84.4% | 3 | Unit, UnitCommand, Unitset | yes |
| 107 | `buildScore` | 16 | 84.6% | 2 | UnitType | yes |
| 108 | `isRefinery` | 16 | 84.8% | 3 | UnitType | yes |
| 109 | `isStartingAttack` | 16 | 85.0% | 5 | Unit | yes |
| 110 | `getAddon` | 15 | 85.2% | 4 | Unit | yes |
| 111 | `getInitialResources` | 15 | 85.4% | 4 | Unit | yes |
| 112 | `isNeutral` | 15 | 85.5% | 6 | Player, UnitType | yes |
| 113 | `isPowered` | 15 | 85.7% | 5 | Unit | yes |
| 114 | `isVisible` | 15 | 85.9% | 4 | Bullet, Game, Unit | yes |
| 115 | `canMove` | 14 | 86.1% | 2 | Unit, UnitType | yes |
| 116 | `elapsedTime` | 14 | 86.2% | 3 | Game | yes |
| 117 | `getPlayers` | 14 | 86.4% | 5 | Force, Forceset | yes |
| 118 | `getSupplyProvider` | 14 | 86.6% | 6 | Race | yes |
| 119 | `getUnitsOnTile` | 14 | 86.7% | 3 | Game | yes |
| 120 | `isCarryingGas` | 14 | 86.9% | 4 | Unit | yes |
| 121 | `isSpell` | 14 | 87.1% | 1 | UnitType | yes |
| 122 | `setLocalSpeed` | 14 | 87.2% | 4 | Game | yes |
| 123 | `isInWeaponRange` | 13 | 87.4% | 2 | Unit | yes |
| 124 | `enableFlag` | 12 | 87.5% | 6 | Game | yes |
| 125 | `getBestUnit` | 12 | 87.7% | 1 | Game | yes |
| 126 | `isEnemy` | 12 | 87.8% | 3 | Player | yes |
| 127 | `isInGame` | 12 | 88.0% | 4 | Game | yes |
| 128 | `isIrradiated` | 12 | 88.1% | 4 | Unit | yes |
| 129 | `isTraining` | 12 | 88.2% | 4 | Unit | yes |
| 130 | `setTextSize` | 12 | 88.4% | 2 | Game | yes |
| 131 | `upgrade` | 12 | 88.5% | 5 | Unit, UnitCommand | yes |
| 132 | `canUpgrade` | 11 | 88.7% | 4 | Game, Unit | yes |
| 133 | `completedUnitCount` | 11 | 88.8% | 3 | Player | yes |
| 134 | `getLoadedUnits` | 11 | 88.9% | 4 | Unit, Unitset | yes |
| 135 | `getTrainingQueue` | 11 | 89.1% | 5 | Unit | yes |
| 136 | `getUnitsInRadius` | 11 | 89.2% | 2 | Game, Unit, Unitset | yes |
| 137 | `isAttacking` | 11 | 89.3% | 3 | Unit | yes |
| 138 | `isOrganic` | 11 | 89.5% | 3 | UnitType | yes |
| 139 | `isTwoUnitsInOneEgg` | 11 | 89.6% | 5 | UnitType | yes |
| 140 | `mapHash` | 11 | 89.7% | 3 | Game | yes |
| 141 | `spaceProvided` | 11 | 89.8% | 4 | UnitType | yes |
| 142 | `transform` | 11 | 90.0% | 3 | BulletType, DamageType, Error, ExplosionType, GameType, Order, PlayerType, Race, TechType, UnitCommandType, UnitSizeType, UnitType, UpgradeType, WeaponType | yes |
| 143 | `buildAddon` | 10 | 90.1% | 4 | Unit, UnitCommand, Unitset | yes |
| 144 | `canBurrow` | 10 | 90.2% | 1 | Unit | yes |
| 145 | `cancelMorph` | 10 | 90.3% | 2 | Unit, UnitCommand, Unitset | yes |
| 146 | `damageCooldown` | 10 | 90.4% | 3 | WeaponType | yes |
| 147 | `destroyScore` | 10 | 90.6% | 2 | UnitType | yes |
| 148 | `getOrderTarget` | 10 | 90.7% | 4 | Unit | yes |
| 149 | `getResourceDepot` | 10 | 90.8% | 3 | Race | yes |
| 150 | `getWeapon` | 10 | 90.9% | 3 | TechType | yes |
| 151 | `isInvincible` | 10 | 91.0% | 1 | Unit, UnitType | yes |
| 152 | `stop` | 10 | 91.2% | 3 | Unit, UnitCommand, Unitset | yes |
| 153 | `canCancelMorph` | 9 | 91.3% | 2 | Unit | yes |
| 154 | `canTrain` | 9 | 91.4% | 2 | Unit | yes |
| 155 | `gasPrice` | 9 | 91.5% | 3 | TechType, UnitType, UpgradeType | yes |
| 156 | `getAngle` | 9 | 91.6% | 4 | Bullet, Unit | yes |
| 157 | `getCenter` | 9 | 91.7% | 1 | Race, Region, Regionset |  |
| 158 | `getLatencyFrames` | 9 | 91.8% | 3 | Game | yes |
| 159 | `getMaxUpgradeLevel` | 9 | 91.9% | 2 | Player | yes |
| 160 | `getRemainingTrainTime` | 9 | 92.0% | 4 | Unit | yes |
| 161 | `getTargetPosition` | 9 | 92.1% | 2 | Bullet, Unit, UnitCommand | yes |
| 162 | `isMechanical` | 9 | 92.2% | 3 | UnitType | yes |
| 163 | `isStimmed` | 9 | 92.3% | 3 | Unit | yes |
| 164 | `isUnderDarkSwarm` | 9 | 92.4% | 1 | Unit |  |
| 165 | `damage` | 8 | 92.5% | 2 | Player | yes |
| 166 | `getRefinery` | 8 | 92.6% | 4 | Race | yes |
| 167 | `getUpgrade` | 8 | 92.7% | 3 | Unit | yes |
| 168 | `isMineralField` | 8 | 92.8% | 3 | UnitType | yes |
| 169 | `isUnderAttack` | 8 | 92.9% | 3 | Unit | yes |
| 170 | `spaceRequired` | 8 | 93.0% | 4 | UnitType | yes |
| 171 | `supplyProvided` | 8 | 93.1% | 3 | UnitType | yes |
| 172 | `toString` | 8 | 93.2% | 2 | BulletType, DamageType, Error, ExplosionType, GameType, Order, PlayerType, Race, TechType, UnitCommandType, UnitSizeType, UnitType, UpgradeType, WeaponType | yes |
| 173 | `unsiege` | 8 | 93.3% | 4 | Unit, UnitCommand, Unitset | yes |
| 174 | `allUnitCount` | 7 | 93.4% | 2 | Player | yes |
| 175 | `canUseTech` | 7 | 93.5% | 2 | Unit | yes |
| 176 | `cancelConstruction` | 7 | 93.6% | 4 | Unit, UnitCommand, Unitset | yes |
| 177 | `getAcidSporeCount` | 7 | 93.6% | 2 | Unit | yes |
| 178 | `getInitialType` | 7 | 93.7% | 1 | Unit |  |
| 179 | `getRegion` | 7 | 93.8% | 2 | Game, Unit | yes |
| 180 | `getTech` | 7 | 93.9% | 3 | Unit, WeaponType | yes |
| 181 | `getTop` | 7 | 94.0% | 2 | Unit | yes |
| 182 | `isBraking` | 7 | 94.1% | 2 | Unit | yes |
| 183 | `isCloakable` | 7 | 94.1% | 4 | UnitType | yes |
| 184 | `isLockedDown` | 7 | 94.2% | 3 | Unit | yes |
| 185 | `isMaelstrommed` | 7 | 94.3% | 3 | Unit | yes |
| 186 | `requiresPsi` | 7 | 94.4% | 4 | UnitType | yes |
| 187 | `build` | 6 | 94.5% | 5 | Unit, UnitCommand, Unitset | yes |
| 188 | `canMorph` | 6 | 94.5% | 2 | Unit | yes |
| 189 | `canProduce` | 6 | 94.6% | 2 | UnitType | yes |
| 190 | `damageAmount` | 6 | 94.7% | 2 | WeaponType | yes |
| 191 | `gatheredGas` | 6 | 94.7% | 1 | Player |  |
| 192 | `getBuildingScore` | 6 | 94.8% | 3 | Player | yes |
| 193 | `getInitialHitPoints` | 6 | 94.9% | 4 | Unit | yes |
| 194 | `getKillScore` | 6 | 95.0% | 3 | Player | yes |
| 195 | `getRight` | 6 | 95.0% | 2 | Unit | yes |
| 196 | `getUnitScore` | 6 | 95.1% | 3 | Player | yes |
| 197 | `isFlyingBuilding` | 6 | 95.2% | 2 | UnitType | yes |
| 198 | `isGatheringMinerals` | 6 | 95.2% | 4 | Unit | yes |
| 199 | `isPlagued` | 6 | 95.3% | 2 | Unit | yes |
| 200 | `isUnderStorm` | 6 | 95.4% | 3 | Unit | yes |
| 201 | `repair` | 6 | 95.5% | 4 | Unit, UnitCommand, Unitset | yes |
| 202 | `seekRange` | 6 | 95.5% | 2 | UnitType | yes |
| 203 | `armor` | 5 | 95.6% | 2 | Player, UnitType | yes |
| 204 | `canBuildAddon` | 5 | 95.6% | 1 | Unit, UnitType |  |
| 205 | `canGather` | 5 | 95.7% | 1 | Unit |  |
| 206 | `canUnsiege` | 5 | 95.8% | 2 | Unit | yes |
| 207 | `damageType` | 5 | 95.8% | 1 | WeaponType |  |
| 208 | `getGroundHeight` | 5 | 95.9% | 2 | Game | yes |
| 209 | `getLeft` | 5 | 95.9% | 2 | Unit | yes |
| 210 | `getPowerUp` | 5 | 96.0% | 2 | Unit | yes |
| 211 | `getRemainingLatencyFrames` | 5 | 96.1% | 2 | Game | yes |
| 212 | `getScreenPosition` | 5 | 96.1% | 1 | Game |  |
| 213 | `getTechType` | 5 | 96.2% | 1 | UnitCommand |  |
| 214 | `getUpgradeType` | 5 | 96.2% | 1 | UnitCommand |  |
| 215 | `hasCreep` | 5 | 96.3% | 5 | Game | yes |
| 216 | `isStasised` | 5 | 96.4% | 2 | Unit | yes |
| 217 | `makeValid` | 5 | 96.4% | 1 | Position |  |
| 218 | `maxRepeats` | 5 | 96.5% | 3 | UpgradeType | yes |
| 219 | `siege` | 5 | 96.5% | 4 | Unit, UnitCommand, Unitset | yes |
| 220 | `unburrow` | 5 | 96.6% | 3 | Unit, UnitCommand, Unitset | yes |
| 221 | `unloadAll` | 5 | 96.7% | 4 | Unit, UnitCommand, Unitset | yes |
| 222 | `weaponDamageCooldown` | 5 | 96.7% | 2 | Player | yes |
| 223 | `canRightClick` | 4 | 96.8% | 1 | Unit |  |
| 224 | `canUnburrow` | 4 | 96.8% | 1 | Unit |  |
| 225 | `energyCost` | 4 | 96.9% | 2 | TechType | yes |
| 226 | `gather` | 4 | 96.9% | 2 | Unit, UnitCommand, Unitset | yes |
| 227 | `getBottom` | 4 | 97.0% | 2 | Unit | yes |
| 228 | `getDefenseMatrixPoints` | 4 | 97.0% | 1 | Unit |  |
| 229 | `getIrradiateTimer` | 4 | 97.1% | 2 | Unit | yes |
| 230 | `getKillCount` | 4 | 97.1% | 2 | Unit | yes |
| 231 | `getMousePosition` | 4 | 97.1% | 1 | Game |  |
| 232 | `getTextColor` | 4 | 97.2% | 2 | Player | yes |
| 233 | `getUnitsInRectangle` | 4 | 97.2% | 1 | Game |  |
| 234 | `getVelocityX` | 4 | 97.3% | 2 | Bullet, Unit | yes |
| 235 | `getVelocityY` | 4 | 97.3% | 2 | Bullet, Unit | yes |
| 236 | `isAlly` | 4 | 97.4% | 2 | Player | yes |
| 237 | `isBeingHealed` | 4 | 97.4% | 3 | Unit | yes |
| 238 | `isMorphing` | 4 | 97.5% | 3 | Unit | yes |
| 239 | `isStuck` | 4 | 97.5% | 3 | Unit | yes |
| 240 | `isWalkable` | 4 | 97.6% | 2 | Game | yes |
| 241 | `mapName` | 4 | 97.6% | 3 | Game | yes |
| 242 | `research` | 4 | 97.7% | 4 | Unit, UnitCommand | yes |
| 243 | `setCommandOptimizationLevel` | 4 | 97.7% | 4 | Game | yes |
| 244 | `canBuild` | 3 | 97.8% | 1 | Unit |  |
| 245 | `canCancelConstruction` | 3 | 97.8% | 1 | Unit |  |
| 246 | `canCloak` | 3 | 97.8% | 1 | Unit |  |
| 247 | `canResearch` | 3 | 97.9% | 3 | Game, Unit | yes |
| 248 | `canSiege` | 3 | 97.9% | 2 | Unit | yes |
| 249 | `canStop` | 3 | 97.9% | 1 | Unit |  |
| 250 | `getAverageFPS` | 3 | 98.0% | 3 | Game | yes |
| 251 | `getBuildLocation` | 3 | 98.0% | 2 | Game | yes |
| 252 | `getInterceptorCount` | 3 | 98.0% | 2 | Unit | yes |
| 253 | `getKeyState` | 3 | 98.1% | 1 | Game |  |
| 254 | `getRemainingUpgradeTime` | 3 | 98.1% | 3 | Unit | yes |
| 255 | `getSpaceRemaining` | 3 | 98.1% | 1 | Unit |  |
| 256 | `hasPermanentCloak` | 3 | 98.2% | 1 | UnitType |  |
| 257 | `hasPower` | 3 | 98.2% | 3 | Game | yes |
| 258 | `isAccelerating` | 3 | 98.3% | 1 | Unit |  |
| 259 | `isEnsnared` | 3 | 98.3% | 1 | Unit |  |
| 260 | `isFlagEnabled` | 3 | 98.3% | 1 | Game |  |
| 261 | `isGatheringGas` | 3 | 98.4% | 2 | Unit | yes |
| 262 | `isSpecialBuilding` | 3 | 98.4% | 2 | UnitType | yes |
| 263 | `isUnderDisruptionWeb` | 3 | 98.4% | 1 | Unit |  |
| 264 | `isWinner` | 3 | 98.5% | 3 | Event | yes |
| 265 | `leaveGame` | 3 | 98.5% | 3 | Game | yes |
| 266 | `load` | 3 | 98.5% | 3 | Unit, UnitCommand, Unitset | yes |
| 267 | `returnCargo` | 3 | 98.6% | 3 | Unit, UnitCommand, Unitset | yes |
| 268 | `supplyRequired` | 3 | 98.6% | 1 | UnitType |  |
| 269 | `burrow` | 2 | 98.6% | 2 | Unit, UnitCommand, Unitset | yes |
| 270 | `canUnloadAtPosition` | 2 | 98.7% | 2 | Unit | yes |
| 271 | `canUseTechPosition` | 2 | 98.7% | 2 | Unit | yes |
| 272 | `damageFactor` | 2 | 98.7% | 1 | WeaponType |  |
| 273 | `drawDotMap` | 2 | 98.7% | 1 | Game |  |
| 274 | `enemies` | 2 | 98.8% | 1 | Game |  |
| 275 | `getBuildUnit` | 2 | 98.8% | 2 | Unit | yes |
| 276 | `getEnsnareTimer` | 2 | 98.8% | 2 | Unit | yes |
| 277 | `getFPS` | 2 | 98.8% | 2 | Game | yes |
| 278 | `getHatchery` | 2 | 98.8% | 1 | Unit |  |
| 279 | `getLarva` | 2 | 98.9% | 1 | Unit, Unitset |  |
| 280 | `getLastError` | 2 | 98.9% | 1 | Game |  |
| 281 | `getLatency` | 2 | 98.9% | 2 | Game | yes |
| 282 | `getLockdownTimer` | 2 | 98.9% | 2 | Unit | yes |
| 283 | `getRazingScore` | 2 | 99.0% | 1 | Player |  |
| 284 | `getRemainingResearchTime` | 2 | 99.0% | 2 | Unit | yes |
| 285 | `getScarabCount` | 2 | 99.0% | 2 | Unit | yes |
| 286 | `getSpellCooldown` | 2 | 99.0% | 1 | Unit |  |
| 287 | `getSpiderMineCount` | 2 | 99.1% | 2 | Unit | yes |
| 288 | `getTransport` | 2 | 99.1% | 2 | Race, Unit | yes |
| 289 | `isBlind` | 2 | 99.1% | 1 | Unit |  |
| 290 | `isBuildable` | 2 | 99.1% | 2 | Game | yes |
| 291 | `isHallucination` | 2 | 99.2% | 2 | Unit | yes |
| 292 | `isHoldingPosition` | 2 | 99.2% | 1 | Unit |  |
| 293 | `isInterruptible` | 2 | 99.2% | 2 | Unit | yes |
| 294 | `isObserver` | 2 | 99.2% | 2 | Player | yes |
| 295 | `isPaused` | 2 | 99.3% | 2 | Game | yes |
| 296 | `isSpellcaster` | 2 | 99.3% | 1 | UnitType |  |
| 297 | `isTargetable` | 2 | 99.3% | 1 | Unit |  |
| 298 | `neutral` | 2 | 99.3% | 2 | Game | yes |
| 299 | `regeneratesHP` | 2 | 99.3% | 1 | UnitType |  |
| 300 | `requiresCreep` | 2 | 99.4% | 2 | UnitType | yes |
| 301 | `researchTime` | 2 | 99.4% | 2 | TechType | yes |
| 302 | `restartGame` | 2 | 99.4% | 1 | Game |  |
| 303 | `setFrameSkip` | 2 | 99.4% | 1 | Game |  |
| 304 | `upgradeTime` | 2 | 99.5% | 2 | UpgradeType | yes |
| 305 | `whatResearches` | 2 | 99.5% | 2 | TechType | yes |
| 306 | `whatUpgrades` | 2 | 99.5% | 2 | UpgradeType | yes |
| 307 | `allies` | 1 | 99.5% | 1 | Game |  |
| 308 | `canCommand` | 1 | 99.5% | 1 | Unit |  |
| 309 | `canHaltConstruction` | 1 | 99.5% | 1 | Unit |  |
| 310 | `canLift` | 1 | 99.6% | 1 | Unit |  |
| 311 | `canReturnCargo` | 1 | 99.6% | 1 | Unit |  |
| 312 | `canUseTechUnit` | 1 | 99.6% | 1 | Unit |  |
| 313 | `getClientVersion` | 1 | 99.6% | 1 | Game |  |
| 314 | `getForce` | 1 | 99.6% | 1 | Game, Player |  |
| 315 | `getGameType` | 1 | 99.6% | 1 | Game |  |
| 316 | `getInstanceNumber` | 1 | 99.6% | 1 | Game |  |
| 317 | `getLatencyTime` | 1 | 99.6% | 1 | Game |  |
| 318 | `getMouseState` | 1 | 99.7% | 1 | Game |  |
| 319 | `getOrder` | 1 | 99.7% | 1 | TechType, Unit |  |
| 320 | `getOrderTargetPosition` | 1 | 99.7% | 1 | Unit |  |
| 321 | `getRallyPosition` | 1 | 99.7% | 1 | Unit |  |
| 322 | `getRandomSeed` | 1 | 99.7% | 1 | Game |  |
| 323 | `getRemoveTimer` | 1 | 99.7% | 1 | Bullet, Unit |  |
| 324 | `getRevision` | 1 | 99.7% | 1 | Game |  |
| 325 | `haltConstruction` | 1 | 99.7% | 1 | Unit, UnitCommand, Unitset |  |
| 326 | `hasPath` | 1 | 99.8% | 1 | Game, Unit |  |
| 327 | `holdPosition` | 1 | 99.8% | 1 | Unit, UnitCommand, Unitset |  |
| 328 | `incompleteUnitCount` | 1 | 99.8% | 1 | Player |  |
| 329 | `isBattleNet` | 1 | 99.8% | 1 | Game |  |
| 330 | `isDebug` | 1 | 99.8% | 1 | Game |  |
| 331 | `isDefenseMatrixed` | 1 | 99.8% | 1 | Unit |  |
| 332 | `isExplored` | 1 | 99.8% | 1 | Game |  |
| 333 | `isGUIEnabled` | 1 | 99.8% | 1 | Game |  |
| 334 | `isLatComEnabled` | 1 | 99.8% | 1 | Game |  |
| 335 | `isMultiplayer` | 1 | 99.9% | 1 | Game |  |
| 336 | `isUnitAvailable` | 1 | 99.9% | 1 | Player |  |
| 337 | `lift` | 1 | 99.9% | 1 | Unit, UnitCommand, Unitset |  |
| 338 | `maxAirHits` | 1 | 99.9% | 1 | UnitType |  |
| 339 | `maxGroundHits` | 1 | 99.9% | 1 | UnitType |  |
| 340 | `minRange` | 1 | 99.9% | 1 | WeaponType |  |
| 341 | `observers` | 1 | 99.9% | 1 | Game |  |
| 342 | `setLatCom` | 1 | 99.9% | 1 | Game |  |
| 343 | `setMap` | 1 | 100.0% | 1 | Game |  |
| 344 | `setScreenPosition` | 1 | 100.0% | 1 | Game |  |
| 345 | `targetsPosition` | 1 | 100.0% | 1 | TechType |  |
| 346 | `targetsUnit` | 1 | 100.0% | 1 | TechType |  |
| 347 | `tileSize` | 1 | 100.0% | 1 | UnitType |  |
