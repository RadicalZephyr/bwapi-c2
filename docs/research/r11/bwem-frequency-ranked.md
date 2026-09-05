# R11.1 appendix: BWEM export candidates, frequency-ranked

Corpus: Stardust, McRave, RazeAndPlunder (bot-owned source only; vendored BWEM copies excluded).
Liberal count — a name is attributed to BWEM if it appears in BWEM's bot-facing headers.
`kind` is `read` (accessor) or `life` (lifecycle/event). Mutators and internals are excluded
from this table entirely and listed at the end.

| # | name | kind | sites | cum % | bots | BWEM class(es) |
|---|---|---|---|---|---|---|
| 1 | `Center` | read | 372 | 27.0% | 3 | Base, ChokePoint, Map |
| 2 | `Get` | read | 263 | 46.0% | 1 | MapImpl |
| 3 | `GetArea` | read | 175 | 58.7% | 2 | Base, Map, MapImpl |
| 4 | `Location` | read | 89 | 65.1% | 3 | Base |
| 5 | `Instance` | read | 88 | 71.5% | 3 | Map |
| 6 | `GetAreas` | read | 64 | 76.2% | 3 | ChokePoint |
| 7 | `ChokePoints` | read | 41 | 79.1% | 3 | Area |
| 8 | `Pos` | read | 33 | 81.5% | 2 | ChokePoint, Neutral |
| 9 | `Areas` | read | 24 | 83.3% | 3 | Map, MapImpl |
| 10 | `GetNearestArea` | read | 21 | 84.8% | 2 | Map, MapImpl |
| 11 | `Geometry` | read | 17 | 86.0% | 3 | ChokePoint |
| 12 | `Top` | read | 17 | 87.2% | 2 | Area |
| 13 | `GetMiniTile` | read | 14 | 88.3% | 2 | Map |
| 14 | `AccessibleFrom` | read | 13 | 89.2% | 1 | Area, ChokePoint |
| 15 | `Altitude` | read | 12 | 90.1% | 2 | MiniTile |
| 16 | `Bases` | read | 12 | 90.9% | 3 | Area |
| 17 | `Geysers` | read | 12 | 91.8% | 2 | Area, Base, Map, MapImpl |
| 18 | `Unit` | read | 12 | 92.7% | 2 | Neutral |
| 19 | `AccessibleNeighbours` | read | 11 | 93.5% | 1 | Area |
| 20 | `GetPath` | read | 10 | 94.2% | 3 | Map, MapImpl |
| 21 | `TopLeft` | read | 10 | 94.9% | 3 | Area, Neutral |
| 22 | `Minerals` | read | 8 | 95.5% | 2 | Area, Base, Map, MapImpl |
| 23 | `Blocked` | read | 7 | 96.0% | 2 | ChokePoint, MiniTile |
| 24 | `StartingLocations` | read | 6 | 96.4% | 1 | Map, MapImpl |
| 25 | `Walkable` | read | 6 | 96.9% | 2 | MiniTile, Tile |
| 26 | `Starting` | read | 4 | 97.2% | 1 | Base |
| 27 | `EnableAutomaticPathAnalysis` | life | 3 | 97.4% | 3 | Map, MapImpl |
| 28 | `FindBasesForStartingLocations` | life | 3 | 97.6% | 3 | Map, MapImpl |
| 29 | `Id` | read | 3 | 97.8% | 1 | Area |
| 30 | `Initialize` | life | 3 | 98.0% | 3 | Map, MapImpl |
| 31 | `OnStaticBuildingDestroyed` | life | 3 | 98.3% | 3 | Map, MapImpl |
| 32 | `StaticBuildings` | read | 3 | 98.5% | 1 | Map, MapImpl |
| 33 | `Amount` | read | 2 | 98.6% | 1 | Ressource |
| 34 | `AreaId` | read | 2 | 98.8% | 1 | MiniTile, Tile |
| 35 | `BlockingNeutral` | read | 2 | 98.9% | 1 | ChokePoint |
| 36 | `GetTile` | read | 2 | 99.1% | 1 | Map |
| 37 | `InitialAmount` | read | 2 | 99.2% | 1 | Ressource |
| 38 | `OnMineralDestroyed` | life | 2 | 99.3% | 2 | Area, Base, Map, MapImpl |
| 39 | `Size` | read | 2 | 99.5% | 1 | Map, Neutral |
| 40 | `BoundingBoxSize` | read | 1 | 99.6% | 1 | Area |
| 41 | `Buildable` | read | 1 | 99.6% | 1 | Tile |
| 42 | `GetNeutral` | read | 1 | 99.7% | 1 | Tile |
| 43 | `Initialized` | life | 1 | 99.8% | 1 | Map |
| 44 | `IsGeyser` | read | 1 | 99.9% | 1 | Geyser, Neutral |
| 45 | `MaxAltitude` | read | 1 | 99.9% | 1 | Area, Map, MapImpl |
| 46 | `NextStacked` | read | 1 | 100.0% | 1 | Neutral |
| 47 | `AltitudeMissing` | read | 0 | 100.0% | 0 | MiniTile |
| 48 | `AreaIdMissing` | read | 0 | 100.0% | 0 | MiniTile |
| 49 | `AutomaticPathUpdate` | read | 0 | 100.0% | 0 | Map, MapImpl |
| 50 | `BaseCount` | read | 0 | 100.0% | 0 | Map, MapImpl |
| 51 | `BlockedAreas` | read | 0 | 100.0% | 0 | Neutral |
| 52 | `Blocking` | read | 0 | 100.0% | 0 | Neutral |
| 53 | `BlockingMinerals` | read | 0 | 100.0% | 0 | Base |
| 54 | `BottomRight` | read | 0 | 100.0% | 0 | Area, Neutral |
| 55 | `ChokePointCount` | read | 0 | 100.0% | 0 | Map, MapImpl |
| 56 | `Crop` | read | 0 | 100.0% | 0 | Map |
| 57 | `DistanceFrom` | read | 0 | 100.0% | 0 | ChokePoint |
| 58 | `Doodad` | read | 0 | 100.0% | 0 | Tile |
| 59 | `GetGeyser` | read | 0 | 100.0% | 0 | Map, MapImpl |
| 60 | `GetMineral` | read | 0 | 100.0% | 0 | Map, MapImpl |
| 61 | `GetPathTo` | read | 0 | 100.0% | 0 | ChokePoint |
| 62 | `GroundHeight` | read | 0 | 100.0% | 0 | Tile |
| 63 | `GroupId` | read | 0 | 100.0% | 0 | Area |
| 64 | `HighGroundPercentage` | read | 0 | 100.0% | 0 | Area |
| 65 | `Index` | read | 0 | 100.0% | 0 | ChokePoint |
| 66 | `IsMineral` | read | 0 | 100.0% | 0 | Mineral, Neutral |
| 67 | `IsPseudo` | read | 0 | 100.0% | 0 | ChokePoint |
| 68 | `IsRessource` | read | 0 | 100.0% | 0 | Neutral, Ressource |
| 69 | `IsStaticBuilding` | read | 0 | 100.0% | 0 | Neutral, StaticBuilding |
| 70 | `Lake` | read | 0 | 100.0% | 0 | MiniTile |
| 71 | `LastStacked` | read | 0 | 100.0% | 0 | Neutral |
| 72 | `LowGroundPercentage` | read | 0 | 100.0% | 0 | Area |
| 73 | `MinAltitude` | read | 0 | 100.0% | 0 | Tile |
| 74 | `MiniTiles` | read | 0 | 100.0% | 0 | Area, Map |
| 75 | `OnBlockingNeutralDestroyed` | life | 0 | 100.0% | 0 | ChokePoint |
| 76 | `PosInArea` | read | 0 | 100.0% | 0 | ChokePoint |
| 77 | `RandomPosition` | read | 0 | 100.0% | 0 | Map |
| 78 | `Sea` | read | 0 | 100.0% | 0 | MiniTile |
| 79 | `SeaOrLake` | read | 0 | 100.0% | 0 | MiniTile |
| 80 | `StackedNeutrals` | read | 0 | 100.0% | 0 | Tile |
| 81 | `Terrain` | read | 0 | 100.0% | 0 | MiniTile, Tile |
| 82 | `Tiles` | read | 0 | 100.0% | 0 | Map |
| 83 | `Type` | read | 0 | 100.0% | 0 | Neutral |
| 84 | `Valid` | read | 0 | 100.0% | 0 | Map |
| 85 | `VeryHighGroundPercentage` | read | 0 | 100.0% | 0 | Area |
| 86 | `WalkSize` | read | 0 | 100.0% | 0 | Map |
| 87 | `minAltitudeBottom` | read | 0 | 100.0% | 0 | Tile |
| 88 | `minAltitudeLeft` | read | 0 | 100.0% | 0 | Tile |
| 89 | `minAltitudeRight` | read | 0 | 100.0% | 0 | Tile |
| 90 | `minAltitudeTop` | read | 0 | 100.0% | 0 | Tile |
| 91 | `seaSide` | read | 0 | 100.0% | 0 | Map |

## Excluded: mutators and internals (42)

None is called by any bot in the corpus.

`AddChokePoints`, `AddGeyser`, `AddMineral`, `AddNeutral`, `AddTileInformation`, `BreadthFirstSearch`, `ChokePointsByArea`, `ComputeBaseLocationScore`, `ComputeDistances`, `CreateBases`, `Draw`, `GetGraph`, `GetMap`, `GetMiniTile_`, `GetTile_`, `InternalData`, `PathBackTrace`, `PostCollectInformation`, `PutOnTiles`, `RawFrontier`, `RemoveFromTiles`, `RemoveNeutral`, `ReplaceAreaId`, `ReplaceBlockedAreaId`, `ResetAreaId`, `SetAltitude`, `SetAreaId`, `SetBlocked`, `SetBlocking`, `SetBuildable`, `SetDoodad`, `SetGroundHeight`, `SetGroupId`, `SetInternalData`, `SetLake`, `SetMinAltitude`, `SetPathBackTrace`, `SetSea`, `SetStartingLocation`, `SetWalkable`, `UpdateAccessibleNeighbours`, `ValidateBaseLocation`
