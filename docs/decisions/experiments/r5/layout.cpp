// Standalone reproduction of BWAPI's shared-memory layout.
// Struct bodies copied verbatim from bwapi/include/BWAPI/Client/*.h.
// Only substitution: BWAPI::UnitCommandType, whose sole non-static data member
// is `int tid` (BWAPI/Type.h:29); static members do not affect layout.
namespace BWAPI { struct UnitCommandType { int tid; }; }

namespace BWAPI
{
  /// <summary>Contains flag enumerations for BWAPI.</summary>
  /// @see Game::enableFlag, Game::isFlagEnabled
  namespace Flag
  {
    /// <summary>Contains flag enumerations for BWAPI.</summary>
    /// @see Game::enableFlag, Game::isFlagEnabled
    enum Enum
    {
      /// <summary>Enable to get information about all units on the map, not just the visible units.</summary>
      CompleteMapInformation = 0,

      /// <summary>Enable to get information from the user (what units are selected, chat messages
      /// the user enters, etc)</summary>
      UserInput              = 1,

      /// <summary>The maximum number of different flags available.</summary>
      Max
    };
  }
}


namespace BWAPI
{
  /// <summary>An enumeration of mouse button inputs.</summary>
  /// @see Game::getMouseState
  enum MouseButton
  {
    M_LEFT = 0,
    M_RIGHT,
    M_MIDDLE,
    M_MAX,
  };

  /// <summary>An enumeration of keyboard input values.</summary>
  /// @see Game::getKeyState
  enum Key
  {
    K_LBUTTON = 0x01,
    K_RBUTTON,
    K_CANCEL,
    K_MBUTTON,
    K_XBUTTON1,
    K_XBUTTON2,
    __UNDEFINED_7,
    K_BACK,
    K_TAB,
    __RESERVED_A,
    __RESERVED_B,
    K_CLEAR,
    K_RETURN,
    __UNDEFINED_E,
    __UNDEFINED_F,
    K_SHIFT,
    K_CONTROL,
    K_MENU,
    K_PAUSE,
    K_CAPITAL,
    K_KANA,
    K_UNDEFINED_16,
    K_JUNJA,
    K_FINAL,
    K_KANJI,
    __UNDEFINED_1A,
    K_ESCAPE,
    K_CONVERT,
    K_NONCONVERT,
    K_ACCEPT,
    K_MODECHANGE,
    K_SPACE,
    K_PRIOR,
    K_NEXT,
    K_END,
    K_HOME,
    K_LEFT,
    K_UP,
    K_RIGHT,
    K_DOWN,
    K_SELECT,
    K_PRINT,
    K_EXECUTE,
    K_SNAPSHOT,
    K_INSERT,
    K_DELETE,
    K_HELP,
    K_0,
    K_1,
    K_2,
    K_3,
    K_4,
    K_5,
    K_6,
    K_7,
    K_8,
    K_9,
    __UNDEFINED_3A,
    __UNDEFINED_3B,
    __UNDEFINED_3C,
    __UNDEFINED_3D,
    __UNDEFINED_3E,
    __UNDEFINED_3F,
    __UNDEFINED_40,
    K_A,
    K_B,
    K_C,
    K_D,
    K_E,
    K_F,
    K_G,
    K_H,
    K_I,
    K_J,
    K_K,
    K_L,
    K_M,
    K_N,
    K_O,
    K_P,
    K_Q,
    K_R,
    K_S,
    K_T,
    K_U,
    K_V,
    K_W,
    K_X,
    K_Y,
    K_Z,
    K_LWIN,
    K_RWIN,
    K_APPS,
    __RESERVED_5E,
    K_SLEEP,
    K_NUMPAD0,
    K_NUMPAD1,
    K_NUMPAD2,
    K_NUMPAD3,
    K_NUMPAD4,
    K_NUMPAD5,
    K_NUMPAD6,
    K_NUMPAD7,
    K_NUMPAD8,
    K_NUMPAD9,
    K_MULTIPLY,
    K_ADD,
    K_SEPARATOR,
    K_SUBTRACT,
    K_DECIMAL,
    K_DIVIDE,
    K_F1,
    K_F2,
    K_F3,
    K_F4,
    K_F5,
    K_F6,
    K_F7,
    K_F8,
    K_F9,
    K_F10,
    K_F11,
    K_F12,
    K_F13,
    K_F14,
    K_F15,
    K_F16,
    K_F17,
    K_F18,
    K_F19,
    K_F20,
    K_F21,
    K_F22,
    K_F23,
    K_F24,
    __UNASSIGNED_88,
    __UNASSIGNED_89,
    __UNASSIGNED_8A,
    __UNASSIGNED_8B,
    __UNASSIGNED_8C,
    __UNASSIGNED_8D,
    __UNASSIGNED_8E,
    __UNASSIGNED_8F,
    K_NUMLOCK,
    K_SCROLL,

    K_OEM_NEC_EQUAL,
    K_OEM_FJ_JISHO,
    K_OEM_FJ_MASSHOU,
    K_OEM_FJ_TOUROKU,
    K_OEM_FJ_LOYA,
    __UNASSIGNED_97,
    __UNASSIGNED_98,
    __UNASSIGNED_99,
    __UNASSIGNED_9A,
    __UNASSIGNED_9B,
    __UNASSIGNED_9C,
    __UNASSIGNED_9D,
    __UNASSIGNED_9E,
    __UNASSIGNED_9F,
    K_LSHIFT,
    K_RSHIFT,
    K_LCONTROL,
    K_RCONTROL,
    K_LMENU,
    K_RMENU,
    K_BROWSER_BACK,
    K_BROWSER_FORWARD,
    K_BROWSER_REFRESH,
    K_BROWSER_STOP,
    K_BROWSER_SEARCH,
    K_BROWSER_FAVORITES,
    K_BROWSER_HOME,
    K_VOLUME_MUTE,
    K_VOLUME_DOWN,
    K_VOLUME_UP,
    K_MEDIA_NEXT_TRACK,
    K_MEDIA_PREV_TRACK,
    K_MEDIA_STOP,
    K_MEDIA_PLAY_PAUSE,
    K_LAUNCH_MAIL,
    K_LAUNCH_MEDIA_SELECT,
    K_LAUNCH_APP1,
    K_LAUNCH_APP2,
    __RESERVED_B8,
    __RESERVED_B9,
    K_OEM_1,
    K_OEM_PLUS,
    K_OEM_COMMA,
    K_OEM_MINUS,
    K_OEM_PERIOD,
    K_OEM_2,
    K_OEM_3,

    K_OEM_4 = 0xDB,
    K_OEM_5,
    K_OEM_6,
    K_OEM_7,
    K_OEM_8,
    __RESERVED_E0,
    K_OEM_AX,
    K_OEM_102,
    K_ICO_HELP,
    K_ICO_00,
    K_PROCESSKEY,
    K_ICO_CLEAR,
    K_PACKET,
    __UNASSIGNED_E8,
    K_OEM_RESET,
    K_OEM_JUMP,
    K_OEM_PA1,
    K_OEM_PA2,
    K_OEM_PA3,
    K_OEM_WSCTRL,
    K_OEM_CUSEL,
    K_OEM_ATTN,
    K_OEM_FINISH,
    K_OEM_COPY,
    K_OEM_AUTO,
    K_OEM_ENLW,
    K_OEM_BACKTAB,

    K_ATTN,
    K_CRSEL,
    K_EXSEL,
    K_EREOF,
    K_PLAY,
    K_ZOOM,
    K_NONAME,
    K_PA1,
    K_OEM_CLEAR,
    K_MAX
  };
}

namespace BWAPI { namespace UnitTypes {

    /// <summary>Enumeration of unit types</summary>
    /// @see UnitType
    namespace Enum
    {
      /// <summary>Enumeration of unit types</summary>
      /// @see UnitType
      enum Enum
      {
        Terran_Marine = 0,
        Terran_Ghost,
        Terran_Vulture,
        Terran_Goliath,
        Terran_Goliath_Turret,
        Terran_Siege_Tank_Tank_Mode,
        Terran_Siege_Tank_Tank_Mode_Turret,
        Terran_SCV,
        Terran_Wraith,
        Terran_Science_Vessel,
        Hero_Gui_Montag,
        Terran_Dropship,
        Terran_Battlecruiser,
        Terran_Vulture_Spider_Mine,
        Terran_Nuclear_Missile,
        Terran_Civilian,
        Hero_Sarah_Kerrigan,
        Hero_Alan_Schezar,
        Hero_Alan_Schezar_Turret,
        Hero_Jim_Raynor_Vulture,
        Hero_Jim_Raynor_Marine,
        Hero_Tom_Kazansky,
        Hero_Magellan,
        Hero_Edmund_Duke_Tank_Mode,
        Hero_Edmund_Duke_Tank_Mode_Turret,
        Hero_Edmund_Duke_Siege_Mode,
        Hero_Edmund_Duke_Siege_Mode_Turret,
        Hero_Arcturus_Mengsk,
        Hero_Hyperion,
        Hero_Norad_II,
        Terran_Siege_Tank_Siege_Mode,
        Terran_Siege_Tank_Siege_Mode_Turret,
        Terran_Firebat,
        Spell_Scanner_Sweep,
        Terran_Medic,
        Zerg_Larva,
        Zerg_Egg,
        Zerg_Zergling,
        Zerg_Hydralisk,
        Zerg_Ultralisk,
        Zerg_Broodling,
        Zerg_Drone,
        Zerg_Overlord,
        Zerg_Mutalisk,
        Zerg_Guardian,
        Zerg_Queen,
        Zerg_Defiler,
        Zerg_Scourge,
        Hero_Torrasque,
        Hero_Matriarch,
        Zerg_Infested_Terran,
        Hero_Infested_Kerrigan,
        Hero_Unclean_One,
        Hero_Hunter_Killer,
        Hero_Devouring_One,
        Hero_Kukulza_Mutalisk,
        Hero_Kukulza_Guardian,
        Hero_Yggdrasill,
        Terran_Valkyrie,
        Zerg_Cocoon,
        Protoss_Corsair,
        Protoss_Dark_Templar,
        Zerg_Devourer,
        Protoss_Dark_Archon,
        Protoss_Probe,
        Protoss_Zealot,
        Protoss_Dragoon,
        Protoss_High_Templar,
        Protoss_Archon,
        Protoss_Shuttle,
        Protoss_Scout,
        Protoss_Arbiter,
        Protoss_Carrier,
        Protoss_Interceptor,
        Hero_Dark_Templar,
        Hero_Zeratul,
        Hero_Tassadar_Zeratul_Archon,
        Hero_Fenix_Zealot,
        Hero_Fenix_Dragoon,
        Hero_Tassadar,
        Hero_Mojo,
        Hero_Warbringer,
        Hero_Gantrithor,
        Protoss_Reaver,
        Protoss_Observer,
        Protoss_Scarab,
        Hero_Danimoth,
        Hero_Aldaris,
        Hero_Artanis,
        Critter_Rhynadon,
        Critter_Bengalaas,
        Special_Cargo_Ship,
        Special_Mercenary_Gunship,
        Critter_Scantid,
        Critter_Kakaru,
        Critter_Ragnasaur,
        Critter_Ursadon,
        Zerg_Lurker_Egg,
        Hero_Raszagal,
        Hero_Samir_Duran,
        Hero_Alexei_Stukov,
        Special_Map_Revealer,
        Hero_Gerard_DuGalle,
        Zerg_Lurker,
        Hero_Infested_Duran,
        Spell_Disruption_Web,
        Terran_Command_Center,
        Terran_Comsat_Station,
        Terran_Nuclear_Silo,
        Terran_Supply_Depot,
        Terran_Refinery,
        Terran_Barracks,
        Terran_Academy,
        Terran_Factory,
        Terran_Starport,
        Terran_Control_Tower,
        Terran_Science_Facility,
        Terran_Covert_Ops,
        Terran_Physics_Lab,
        Unused_Terran1,
        Terran_Machine_Shop,
        Unused_Terran2,
        Terran_Engineering_Bay,
        Terran_Armory,
        Terran_Missile_Turret,
        Terran_Bunker,
        Special_Crashed_Norad_II,
        Special_Ion_Cannon,
        Powerup_Uraj_Crystal,
        Powerup_Khalis_Crystal,
        Zerg_Infested_Command_Center,
        Zerg_Hatchery,
        Zerg_Lair,
        Zerg_Hive,
        Zerg_Nydus_Canal,
        Zerg_Hydralisk_Den,
        Zerg_Defiler_Mound,
        Zerg_Greater_Spire,
        Zerg_Queens_Nest,
        Zerg_Evolution_Chamber,
        Zerg_Ultralisk_Cavern,
        Zerg_Spire,
        Zerg_Spawning_Pool,
        Zerg_Creep_Colony,
        Zerg_Spore_Colony,
        Unused_Zerg1,
        Zerg_Sunken_Colony,
        Special_Overmind_With_Shell,
        Special_Overmind,
        Zerg_Extractor,
        Special_Mature_Chrysalis,
        Special_Cerebrate,
        Special_Cerebrate_Daggoth,
        Unused_Zerg2,
        Protoss_Nexus,
        Protoss_Robotics_Facility,
        Protoss_Pylon,
        Protoss_Assimilator,
        Unused_Protoss1,
        Protoss_Observatory,
        Protoss_Gateway,
        Unused_Protoss2,
        Protoss_Photon_Cannon,
        Protoss_Citadel_of_Adun,
        Protoss_Cybernetics_Core,
        Protoss_Templar_Archives,
        Protoss_Forge,
        Protoss_Stargate,
        Special_Stasis_Cell_Prison,
        Protoss_Fleet_Beacon,
        Protoss_Arbiter_Tribunal,
        Protoss_Robotics_Support_Bay,
        Protoss_Shield_Battery,
        Special_Khaydarin_Crystal_Form,
        Special_Protoss_Temple,
        Special_XelNaga_Temple,
        Resource_Mineral_Field,
        Resource_Mineral_Field_Type_2,
        Resource_Mineral_Field_Type_3,
        Unused_Cave,
        Unused_Cave_In,
        Unused_Cantina,
        Unused_Mining_Platform,
        Unused_Independant_Command_Center,
        Special_Independant_Starport,
        Unused_Independant_Jump_Gate,
        Unused_Ruins,
        Unused_Khaydarin_Crystal_Formation,
        Resource_Vespene_Geyser,
        Special_Warp_Gate,
        Special_Psi_Disrupter,
        Unused_Zerg_Marker,
        Unused_Terran_Marker,
        Unused_Protoss_Marker,
        Special_Zerg_Beacon,
        Special_Terran_Beacon,
        Special_Protoss_Beacon,
        Special_Zerg_Flag_Beacon,
        Special_Terran_Flag_Beacon,
        Special_Protoss_Flag_Beacon,
        Special_Power_Generator,
        Special_Overmind_Cocoon,
        Spell_Dark_Swarm,
        Special_Floor_Missile_Trap,
        Special_Floor_Hatch,
        Special_Upper_Level_Door,
        Special_Right_Upper_Level_Door,
        Special_Pit_Door,
        Special_Right_Pit_Door,
        Special_Floor_Gun_Trap,
        Special_Wall_Missile_Trap,
        Special_Wall_Flame_Trap,
        Special_Right_Wall_Missile_Trap,
        Special_Right_Wall_Flame_Trap,
        Special_Start_Location,
        Powerup_Flag,
        Powerup_Young_Chrysalis,
        Powerup_Psi_Emitter,
        Powerup_Data_Disk,
        Powerup_Khaydarin_Crystal,
        Powerup_Mineral_Cluster_Type_1,
        Powerup_Mineral_Cluster_Type_2,
        Powerup_Protoss_Gas_Orb_Type_1,
        Powerup_Protoss_Gas_Orb_Type_2,
        Powerup_Zerg_Gas_Sac_Type_1,
        Powerup_Zerg_Gas_Sac_Type_2,
        Powerup_Terran_Gas_Tank_Type_1,
        Powerup_Terran_Gas_Tank_Type_2,

        None,
        AllUnits,
        Men,
        Buildings,
        Factories,
        Unknown,
        MAX
      };
} } }

namespace BWAPI { namespace TechTypes {

    /// <summary>Enumeration of Tech Types.</summary>
    /// @see TechType
    namespace Enum
    {
      /// <summary>Enumeration of Tech Types.</summary>
      /// @see TechType
      enum Enum
      {
        Stim_Packs = 0,
        Lockdown,
        EMP_Shockwave,
        Spider_Mines,
        Scanner_Sweep,
        Tank_Siege_Mode,
        Defensive_Matrix,
        Irradiate,
        Yamato_Gun,
        Cloaking_Field,
        Personnel_Cloaking,
        Burrowing,
        Infestation,
        Spawn_Broodlings,
        Dark_Swarm,
        Plague,
        Consume,
        Ensnare,
        Parasite,
        Psionic_Storm,
        Hallucination,
        Recall,
        Stasis_Field,
        Archon_Warp,
        Restoration,
        Disruption_Web,
        Unused_26,
        Mind_Control,
        Dark_Archon_Meld,
        Feedback,
        Optical_Flare,
        Maelstrom,
        Lurker_Aspect,
        Unused_33,
        Healing,

        None = 44,
        Nuclear_Strike,
        Unknown,
        MAX
      };
} } }

namespace BWAPI { namespace UpgradeTypes {

    /// <summary>Enumeration of upgrade types.</summary>
    namespace Enum
    {
      /// <summary>Enumeration of upgrade types.</summary>
      enum Enum
      {
        Terran_Infantry_Armor   = 0,
        Terran_Vehicle_Plating  = 1,
        Terran_Ship_Plating     = 2,
        Zerg_Carapace           = 3,
        Zerg_Flyer_Carapace     = 4,
        Protoss_Ground_Armor    = 5,
        Protoss_Air_Armor       = 6,
        Terran_Infantry_Weapons = 7,
        Terran_Vehicle_Weapons  = 8,
        Terran_Ship_Weapons     = 9,
        Zerg_Melee_Attacks      = 10,
        Zerg_Missile_Attacks    = 11,
        Zerg_Flyer_Attacks      = 12,
        Protoss_Ground_Weapons  = 13,
        Protoss_Air_Weapons     = 14,
        Protoss_Plasma_Shields  = 15,
        U_238_Shells            = 16,
        Ion_Thrusters           = 17,
    
        Titan_Reactor           = 19,
        Ocular_Implants         = 20,
        Moebius_Reactor         = 21,
        Apollo_Reactor          = 22,
        Colossus_Reactor        = 23,
        Ventral_Sacs            = 24,
        Antennae                = 25,
        Pneumatized_Carapace    = 26,
        Metabolic_Boost         = 27,
        Adrenal_Glands          = 28,
        Muscular_Augments       = 29,
        Grooved_Spines          = 30,
        Gamete_Meiosis          = 31,
        Metasynaptic_Node       = 32,
        Singularity_Charge      = 33,
        Leg_Enhancements        = 34,
        Scarab_Damage           = 35,
        Reaver_Capacity         = 36,
        Gravitic_Drive          = 37,
        Sensor_Array            = 38,
        Gravitic_Boosters       = 39,
        Khaydarin_Amulet        = 40,
        Apial_Sensors           = 41,
        Gravitic_Thrusters      = 42,
        Carrier_Capacity        = 43,
        Khaydarin_Core          = 44,
    
        Argus_Jewel             = 47,
    
        Argus_Talisman          = 49,
    
        Caduceus_Reactor        = 51,
        Chitinous_Plating       = 52,
        Anabolic_Synthesis      = 53,
        Charon_Boosters         = 54,
    
        Upgrade_60              = 60,
        None                    = 61,
        Unknown,
        MAX
      };
} } }


namespace BWAPI
{
  /// <summary>Contains a list of event types supported by BWAPI.</summary>
  namespace EventType
  {
    /// <summary>Enumeration of callback event types.</summary>
    enum Enum
    {
      MatchStart,
      MatchEnd,
      MatchFrame,
      MenuFrame,
      SendText,
      ReceiveText,
      PlayerLeft,
      NukeDetect,
      UnitDiscover,
      UnitEvade,
      UnitShow,
      UnitHide,
      UnitCreate,
      UnitDestroy,
      UnitMorph,
      UnitRenegade,
      SaveGame,
      UnitComplete,
      //TriggerAction,
      None
    };
  }
}


namespace BWAPI
{
  /// <summary>Contains the coordinate type enumeration for relative drawing positions.</summary>
  namespace CoordinateType
  {
    /// <summary>The coordinate type enumeration, used to indicate relative drawing positions.</summary>
    enum Enum
    {
      /// <summary>A default value for uninitialized coordinate types.</summary>
      None      = 0,

      /// <summary>Positions::Origin (0,0) corresponds to the top left corner of the <b>screen</b>.</summary>
      Screen    = 1,

      /// <summary>Positions::Origin (0,0) corresponds to the top left corner of the <b>map</b>.</summary>
      Map       = 2,

      /// <summary>Positions::Origin (0,0) corresponds to the location of the <b>mouse cursor</b>.</summary>
      Mouse     = 3,
    };
  }
}


/**
 *  Used in UnitCommand
 */

namespace BWAPIC
{
  namespace ShapeType
  {
    enum Enum
    {
      None,
      Text,
      Box,
      Triangle,
      Circle,
      Ellipse,
      Dot,
      Line
    };
  }
}




/**
 * UnitOrder contains a single whole order
 */

namespace BWAPIC
{
  struct UnitCommand
  {
    BWAPI::UnitCommandType type;
    int unitIndex;
    int targetIndex;
    int x;
    int y;
    int extra;
  };
}


namespace BWAPI
{
  struct ForceData
  {
    char name[32];
  };
}







namespace BWAPI
{
  struct PlayerData
  {
    char name[25];
    int  race;
    int  type;
    int  force;
    bool isAlly[12];
    bool isEnemy[12];
    bool isNeutral;
    int  startLocationX;
    int  startLocationY;
    bool isVictorious;
    bool isDefeated;
    bool leftGame;
    bool isParticipating;

    int minerals;
    int gas;
    int gatheredMinerals;
    int gatheredGas;
    int repairedMinerals;
    int repairedGas;
    int refundedMinerals;
    int refundedGas;
    int supplyTotal[3];
    int supplyUsed[3];

    int allUnitCount[UnitTypes::Enum::MAX];
    int visibleUnitCount[UnitTypes::Enum::MAX];
    int completedUnitCount[UnitTypes::Enum::MAX];
    int deadUnitCount[UnitTypes::Enum::MAX];
    int killedUnitCount[UnitTypes::Enum::MAX];

    int  upgradeLevel[UpgradeTypes::Enum::MAX];
    bool hasResearched[TechTypes::Enum::MAX];
    bool isResearching[TechTypes::Enum::MAX];
    bool isUpgrading[UpgradeTypes::Enum::MAX];

    int color;

    int totalUnitScore;
    int totalKillScore;
    int totalBuildingScore;
    int totalRazingScore;
    int customScore;

    int   maxUpgradeLevel[UpgradeTypes::Enum::MAX];
    bool  isResearchAvailable[TechTypes::Enum::MAX];
    bool  isUnitAvailable[UnitTypes::Enum::MAX];
  };
}



namespace BWAPI
{
  struct RegionData
  {
    int   id;
    int   islandID;
    int   center_x;
    int   center_y;
    int   priority;

    // region boundary
    int   leftMost;
    int   rightMost;
    int   topMost;
    int   bottomMost;

    int   neighborCount;
    int   neighbors[256];

    bool  isAccessible;
    bool  isHigherGround;
  };
};



namespace BWAPI
{
  struct UnitData
  {
    int clearanceLevel;
    int id;
    int player;
    int type;
    int positionX;
    int positionY;
    double angle;
    double velocityX;
    double velocityY;
    int hitPoints;
    int lastHitPoints;
    int shields;
    int energy;
    int resources;
    int resourceGroup;

    int killCount;
    int acidSporeCount;
    int scarabCount;
    int interceptorCount;
    int spiderMineCount;
    int groundWeaponCooldown;
    int airWeaponCooldown;
    int spellCooldown;
    int defenseMatrixPoints;

    int defenseMatrixTimer;
    int ensnareTimer;
    int irradiateTimer;
    int lockdownTimer;
    int maelstromTimer;
    int orderTimer;
    int plagueTimer;
    int removeTimer;
    int stasisTimer;
    int stimTimer;

    int buildType;
    int trainingQueueCount;
    int trainingQueue[5];
    int tech;
    int upgrade;
    int remainingBuildTime;
    int remainingTrainTime;
    int remainingResearchTime;
    int remainingUpgradeTime;
    int buildUnit;

    int target;
    int targetPositionX;
    int targetPositionY;
    int order;
    int orderTarget;
    int orderTargetPositionX;
    int orderTargetPositionY;
    int secondaryOrder;
    int rallyPositionX;
    int rallyPositionY;
    int rallyUnit;
    int addon;
    int nydusExit;
    int powerUp;

    int transport;
    int carrier;
    int hatchery;
    
    bool exists;
    bool hasNuke;
    bool isAccelerating;
    bool isAttacking;
    bool isAttackFrame;
    bool isBeingGathered;
    bool isBlind;
    bool isBraking;
    bool isBurrowed;
    int  carryResourceType;
    bool isCloaked;
    bool isCompleted;
    bool isConstructing;
    bool isDetected;
    bool isGathering;
    bool isHallucination;
    bool isIdle;
    bool isInterruptible;
    bool isInvincible;
    bool isLifted;
    bool isMorphing;
    bool isMoving;
    bool isParasited;
    bool isSelected;
    bool isStartingAttack;
    bool isStuck;
    bool isTraining;
    bool isUnderStorm;
    bool isUnderDarkSwarm;
    bool isUnderDWeb;
    bool isPowered;
    bool isVisible[9];
    int  buttonset;

    int  lastAttackerPlayer;
    bool recentlyAttacked;
    int  replayID;
  };
}



namespace BWAPI
{
  struct BulletData
  {
    int id;
    int player;
    int type;
    int source;
    int positionX;
    int positionY;
    double angle;
    double velocityX;
    double velocityY;
    int target;
    int targetPositionX;
    int targetPositionY;
    int removeTimer;
    bool exists;
    bool isVisible[9];
  };
}






namespace BWAPIC
{ 
  struct Event
  {
    BWAPI::EventType::Enum type;
    int v1;
    int v2;
  };
}




/**
 *  Used in UnitCommand
 */

namespace BWAPIC
{
  namespace CommandType
  {
    enum Enum
    {
      None,
      SetScreenPosition,
      PingMinimap,
      EnableFlag,
      Printf,
      SendText,
      PauseGame,
      ResumeGame,
      LeaveGame,
      RestartGame,
      SetLocalSpeed,
      SetLatCom,
      SetGui,
      SetFrameSkip,
      SetMap,
      SetAllies,
      SetVision,
      SetCommandOptimizerLevel,
      SetRevealAll
    };
  }
}
namespace BWAPIC
{
  struct Command
  {
    Command()
     : type( CommandType::None )
     , value1( 0 )
     , value2( 0 )
    { }
    Command(CommandType::Enum _commandType, int _value1=0, int _value2=0)
     : type( _commandType )
     , value1( _value1 )
     , value2( _value2 )
    { }
    
    CommandType::Enum type;
    int value1;
    int value2;
  };
}






namespace BWAPIC
{
  struct Shape
  {
    Shape()
    :type(ShapeType::None)
    ,ctype(BWAPI::CoordinateType::None)
    ,x1(0)
    ,y1(0)
    ,x2(0)
    ,y2(0)
    ,extra1(0)
    ,extra2(0)
    ,color(0)
    ,isSolid(false)
    {
    }

    Shape(ShapeType::Enum _shapeType, BWAPI::CoordinateType::Enum _ctype, int _x1, int _y1, int _x2, int _y2, int _extra1, int _extra2, int _color, bool _isSolid)
    :type(_shapeType)
    ,ctype(_ctype)
    ,x1(_x1)
    ,y1(_y1)
    ,x2(_x2)
    ,y2(_y2)
    ,extra1(_extra1)
    ,extra2(_extra2)
    ,color(_color)
    ,isSolid(_isSolid)
    {
    }

    ShapeType::Enum type;
    BWAPI::CoordinateType::Enum ctype;
    int x1;
    int y1;
    int x2;
    int y2;
    int extra1;
    int extra2;
    int color;
    bool isSolid;
  };
}












namespace BWAPIC
{
  struct Position
  {
    Position() {x=0;y=0;}
    int x;
    int y;
  };
}
namespace BWAPI
{
  struct unitFinder
  {
    int unitIndex;
    int searchValue;
  };
  struct GameData
  {
    //revision and debug will stay at the top of struct so they don't move in memory from revision to revision.
    int client_version;
    int revision;
    bool isDebug;

    int instanceID;
    int botAPM_noselects;
    int botAPM_selects;

    //forces
    int forceCount;
    ForceData forces[5];

    //players
    int playerCount;
    PlayerData players[12];

    //units
    int initialUnitCount;
    UnitData units[10000];

    //unit table
    int unitArray[1700];

    //bullets
    BulletData bullets[100];

    // nuke dots
    int nukeDotCount;
    BWAPIC::Position nukeDots[200];

    int gameType;
    int latency;
    int latencyFrames;
    int latencyTime;
    int remainingLatencyFrames;
    int remainingLatencyTime;
    bool hasLatCom;
    bool hasGUI;
    int replayFrameCount;
    unsigned randomSeed;
    int frameCount;
    int elapsedTime;
    int countdownTimer;
    int fps;
    double averageFPS;

    // user input
    int mouseX;
    int mouseY;
    bool mouseState[M_MAX];
    bool keyState[K_MAX];
    int screenX;
    int screenY;

    bool flags[BWAPI::Flag::Max];

    // map
    int mapWidth;
    int mapHeight;
    char mapFileName[261];  //size based on broodwar memory
    char mapPathName[261];  //size based on broodwar memory
    char mapName[33];      //size based on broodwar memory
    char mapHash[41];

    //tile data
    int  getGroundHeight[256][256];
    bool isWalkable[1024][1024]; 
    bool isBuildable[256][256];
    bool isVisible[256][256];
    bool isExplored[256][256];
    bool hasCreep[256][256];
    bool isOccupied[256][256];

    unsigned short mapTileRegionId[256][256];
    unsigned short mapSplitTilesMiniTileMask[5000];
    unsigned short mapSplitTilesRegion1[5000];
    unsigned short mapSplitTilesRegion2[5000];

    int regionCount;
    RegionData regions[5000];

    // start locations
    int startLocationCount;
    BWAPIC::Position startLocations[8];

    // match mode
    bool isInGame;
    bool isMultiplayer;
    bool isBattleNet;
    bool isPaused;
    bool isReplay;

    //selected units
    int selectedUnitCount;
    int selectedUnits[12];

    // players
    int self;
    int enemy;
    int neutral;

    static const int MAX_EVENTS         = 10000;
    static const int MAX_EVENT_STRINGS  =  1000;
    static const int MAX_STRINGS        = 20000;
    static const int MAX_SHAPES         = 20000;
    static const int MAX_COMMANDS       = 20000;
    static const int MAX_UNIT_COMMANDS  = 20000;

    //events from server to client
    int eventCount;
    BWAPIC::Event events[MAX_EVENTS];

    //strings used in events
    int eventStringCount;
    char eventStrings[MAX_EVENT_STRINGS][256];

    //strings (used in shapes and commands)
    int stringCount;
    char strings[MAX_STRINGS][1024];

    //shapes, commands, unitCommands, from client to server
    int shapeCount;
    BWAPIC::Shape shapes[MAX_SHAPES];

    int commandCount;
    BWAPIC::Command commands[MAX_COMMANDS];

    int unitCommandCount;
    BWAPIC::UnitCommand unitCommands[MAX_UNIT_COMMANDS];

    int unitSearchSize;
    unitFinder xUnitSearch[1700*2];
    unitFinder yUnitSearch[1700*2];
  };
}



namespace BWAPI
{
  struct GameInstance
  {
    GameInstance()
     : serverProcessID(0)
     , isConnected(false)
     , lastKeepAliveTime(0)
    {};
    GameInstance(unsigned int servID, bool connected, unsigned int lastAliveTime)
      : serverProcessID(servID)
      , isConnected(connected)
      , lastKeepAliveTime(lastAliveTime)
    {};

    unsigned int serverProcessID;
    bool isConnected;
    //time_t lastKeepAliveTime;
    unsigned int lastKeepAliveTime;
  };
  struct GameTable
  {
    GameTable()
    {}
    static const int MAX_GAME_INSTANCES = 8;
    GameInstance gameInstances[MAX_GAME_INSTANCES];
  };
}

 BWAPI::GameTable gt;

