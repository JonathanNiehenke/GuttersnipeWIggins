#ifndef GUTTERSNIPEWIGGINS_CPP
#define GUTTERSNIPEWIGGINS_CPP
#include "GuttersnipeWiggins.h"

// ToDo: UnitTrainer.

using namespace BWAPI::Filter;

typedef std::pair<BWAPI::Position, BWAPI::Unitset> PositionedUnits;
typedef std::set<BWAPI::TilePosition> locationSet;


BWAPI::Player SELF;
BWAPI::Unit BASE_CENTER;  // The primary/initial base building.
BWAPI::UnitType CENTER_TYPE, WORKER_TYPE, SUPPLY_TYPE, ARMY_ENABLING_TECH_TYPE,
                ARMY_UNIT_TYPE;
// Indicates number already in construction/training for UnitType.
std::map<BWAPI::UnitType, short> PENDING_UNIT_TYPE_COUNT;

int getNumQueued(BWAPI::Unit trainingFacility)
{
    const bool isZerg = SELF->getRace() == BWAPI::Races::Zerg;
    // Zerg has parallel queue of 3, as opposed to serial queue of 5.
    return isZerg ? 3 - trainingFacility->getLarva().size()
                  : trainingFacility->getTrainingQueue().size();
}

void GW::onStart()
{
    using namespace BWAPI;
    Broodwar->enableFlag(1);
    SELF = Broodwar->self();
    TilePosition startLocation = SELF->getStartLocation();
    BASE_CENTER = Broodwar->getClosestUnit(
        Position(startLocation), IsResourceDepot);
    Race myRace = SELF->getRace();
    CENTER_TYPE = myRace.getCenter();
    SUPPLY_TYPE = myRace.getSupplyProvider();
    WORKER_TYPE = myRace.getWorker();
    // Used only by Zerg.
    // TRAINING[SUPPLY_TYPE].includeFacility(BASE_CENTER);
    switch (myRace) {
        case Races::Enum::Protoss: // Enum for constant value.
            ARMY_ENABLING_TECH_TYPE = UnitTypes::Protoss_Gateway;
            ARMY_UNIT_TYPE = UnitTypes::Protoss_Zealot;
            break;
        case Races::Enum::Terran:
            ARMY_ENABLING_TECH_TYPE = UnitTypes::Terran_Barracks;
            ARMY_UNIT_TYPE = UnitTypes::Terran_Marine;
            break;
        case Races::Enum::Zerg:
            ARMY_ENABLING_TECH_TYPE = UnitTypes::Zerg_Spawning_Pool;
            ARMY_UNIT_TYPE = UnitTypes::Zerg_Zergling;
            break;
    }
    cartographer.discoverResources(BWAPI::Position(startLocation));
    // Workaround cause initial workers may complete before the BASE_CENTER.
    for (BWAPI::Unitset mineralCluster: cartographer.getMinerals()) {
        BWAPI::Unit baseCenter = BWAPI::Broodwar->getClosestUnit(
            mineralCluster.getPosition(), IsResourceDepot, 300);
        if (baseCenter == BASE_CENTER) {
            ecoBaseManager.addBase(BASE_CENTER, mineralCluster);
            break;
        }
    }
    workerBuffer = GW::getUnitBuffer(WORKER_TYPE);
    squadCommander.onStart(BASE_CENTER, ARMY_UNIT_TYPE, SELF, &cartographer);
    buildingConstructer.onStart(SELF, BASE_CENTER, &cmdRescuer, &cartographer);
    warriorTrainer.onStart(ARMY_UNIT_TYPE, &cmdRescuer);
}

void GW::onFrame()
{
    const int actionFrames = std::max(5, BWAPI::Broodwar->getLatency());
    GW::displayStatus(); // For debugging.
    switch(BWAPI::Broodwar->getFrameCount() % actionFrames) {
        case 0: GW::manageProduction();
            break;
        case 1: GW::manageBases();
            break;
        case 2: GW::manageAttackGroups();
            break;
        case 3: squadCommander.combatMicro();
            break;
        case 4:
            cmdRescuer.rescue();
            cartographer.cleanEnemyLocations();
            if (SELF->supplyUsed() == 400) {
                squadCommander.assembleSquad();
            }
            break;
        default: break;
    }
}

void GW::onUnitCreate(BWAPI::Unit Unit)
{
    if (Unit->getPlayer() != SELF) return;  // Ignoring non-owned units
    BWAPI::UnitType unitType = Unit->getType();
    if (unitType.isBuilding()) {
        buildingConstructer.addProduct(Unit);
        if (unitType == ARMY_ENABLING_TECH_TYPE) {
            if (!warriorTrainer.isAvailable())
                scout(cartographer.getStartingLocations());
            warriorTrainer.includeFacility(Unit);
            armyBuffer = getUnitBuffer(ARMY_UNIT_TYPE);
        }
        else if (unitType == SUPPLY_TYPE) {
            squadCommander.assembleSquad();
        }
    }
    PENDING_UNIT_TYPE_COUNT[Unit->getType()]++;
    // Always after change to pending count.
    availableSupply = GW::getAvailableSupply();
}

void GW::onUnitMorph(BWAPI::Unit Unit)
{
    BWAPI::UnitType unitType = Unit->getType();
    if (Unit->getPlayer() != SELF) {
        // Because geyser structures are never destroyed.
        if (unitType == BWAPI::UnitTypes::Resource_Vespene_Geyser) {
            cartographer.removeBuildingLocation(Unit->getTilePosition());
        }
        return;  // Ignoring the other non-owned units.
    }
    if (unitType == BWAPI::UnitTypes::Zerg_Egg) {
        BWAPI::UnitType insideEggType = Unit->getBuildType();
        if (insideEggType == SUPPLY_TYPE &&
                warriorTrainer.isAvailable())
            squadCommander.assembleSquad();
        PENDING_UNIT_TYPE_COUNT[insideEggType]++;
        // Always after change to pending count.
        availableSupply = GW::getAvailableSupply();
    }
    else if (unitType.isBuilding()) {
        buildingConstructer.addProduct(Unit);
        PENDING_UNIT_TYPE_COUNT[unitType]++;
        // Zerg workers become buildings, so recalculate.
        availableSupply = GW::getAvailableSupply();
        if (unitType == BWAPI::UnitTypes::Zerg_Hatchery) {
            // TRAINING[SUPPLY_TYPE].includeFacility(Unit);
            warriorTrainer.includeFacility(Unit);
        }
        else if (unitType == BWAPI::UnitTypes::Zerg_Spawning_Pool) {
            warriorTrainer.includeFacility(BASE_CENTER);
            scout(cartographer.getStartingLocations());
        }
        armyBuffer = getUnitBuffer(ARMY_UNIT_TYPE);
    }
}

void GW::onUnitComplete(BWAPI::Unit Unit)
{
    if (Unit->getPlayer() != SELF)
        return;  // Ignoring non-owned units.
    BWAPI::UnitType unitType = Unit->getType();
    if (unitType.isBuilding()) {
        buildingConstructer.removeConstruction(Unit);
    }
    if (unitType == WORKER_TYPE) {
        try {
            ecoBaseManager.addWorker(Unit);
        }
        catch (char* err) {
            BWAPI::Broodwar->sendText(err);
        }
    }
    else if (unitType == SUPPLY_TYPE) {
        ++supplyCount;
    }
    // Prevent Hatcheries not meant to be a Base;
    // Don't duplicated BASE_CENTER EcoBase, done at onStart.
    else if (unitType == CENTER_TYPE &&
        !Unit->getClosestUnit(IsResourceDepot, 350) && Unit != BASE_CENTER)
    {
        BWAPI::Broodwar->registerEvent(
            [Unit](BWAPI::Game*)
                {
                    BWAPI::Broodwar->drawCircleMap(Unit->getPosition(),
                        350, BWAPI::Color(255, 127, 0), false);
                },
            nullptr, 72);
        BWAPI::Broodwar << "Searching for minerals" << std::endl;
        // Add to ecoBaseManager after finding nearest mineralCluster.
        for (BWAPI::Unitset mineralCluster: cartographer.getMinerals()) {
            BWAPI::Unit baseCenter = BWAPI::Broodwar->getClosestUnit(
                mineralCluster.getPosition(), IsResourceDepot, 300);
            if (baseCenter == Unit) {
                BWAPI::Broodwar << "Creating EcoBase" << std::endl;
                ecoBaseManager.addBase(baseCenter, mineralCluster);
                break;
            }
        }
        workerBuffer = GW::getUnitBuffer(WORKER_TYPE);
    }
    PENDING_UNIT_TYPE_COUNT[unitType]--;
    // Always after change to pending count.
    availableSupply = GW::getAvailableSupply();
}

void GW::onUnitDestroy(BWAPI::Unit Unit)
{
    BWAPI::Player owningPlayer = Unit->getPlayer();
    BWAPI::UnitType unitType = Unit->getType();
    if (owningPlayer == SELF) {
        if (unitType.isBuilding()) {
            buildingConstructer.removeConstruction(Unit);
        }
        if (!Unit->isCompleted())
            PENDING_UNIT_TYPE_COUNT[Unit->getType()]--;
        if (unitType == ARMY_UNIT_TYPE) {
            squadCommander.removeWarrior(Unit);
        }
        else if (unitType == WORKER_TYPE) {
            try {
                ecoBaseManager.removeWorker(Unit);
            }
            catch (char* err) {
                BWAPI::Broodwar->sendText(err);
            }
        }
        else if (unitType == BWAPI::UnitTypes::Zerg_Spawning_Pool ||
                Unit == BASE_CENTER) {
            BWAPI::Broodwar->sendText("gg, you've proven more superior.");
            BWAPI::Broodwar->leaveGame();
        }
        else if (unitType == ARMY_ENABLING_TECH_TYPE) {
            warriorTrainer.removeFacility(Unit);
        }
        else if (unitType.isResourceDepot()) {
            // Includes potential Lair or Hive with isResourceDepot.
            ecoBaseManager.removeBase(Unit);
        }
        else if (unitType == SUPPLY_TYPE && Unit->isCompleted()) {
            --supplyCount;
        }
    }
    else if (unitType.isMineralField()) {
        BWAPI::Broodwar->sendText("Mineral field was destroyed.");
            try {
                ecoBaseManager.removeMineral(Unit);
            }
            catch (char* err) {
                BWAPI::Broodwar->sendText(err);
            }
    }
    else if (unitType.isBuilding()) {
        cartographer.removeBuildingLocation(
            owningPlayer, Unit->getTilePosition());
    }
}

void GW::onUnitDiscover(BWAPI::Unit Unit)
{
    BWAPI::Player owningPlayer = Unit->getPlayer();
    if (SELF->isEnemy(owningPlayer) && Unit->getType().isBuilding() &&
        !Unit->isFlying())
    {
        cartographer.addBuildingLocation(
            owningPlayer, Unit->getTilePosition());
        // BWAPI::Broodwar->sendTextEx(true, "Enemy %s discovered.",
            // Unit->getType().c_str());
    }
}

void GW::onUnitEvade(BWAPI::Unit Unit)
{
}

void GW::onUnitShow(BWAPI::Unit Unit)
{
}

void GW::onUnitHide(BWAPI::Unit Unit)
{
}

void GW::onUnitRenegade(BWAPI::Unit Unit)
{
    // Perhaps I will learn something.
    BWAPI::Broodwar->sendTextEx(true, "%s is Renegade: %s.",
        Unit->getPlayer()->getName().c_str(), Unit->getType().c_str());
    if (Unit->getPlayer() != SELF)
        return;  // Ignoring non-owned units.
}

void GW::onNukeDetect(BWAPI::Position target)
{
}

void GW::onSendText(std::string text)
{
    if (text.substr(0, 7) == "getUnit") {
        int unitId = atoi(text.substr(8, 4).c_str());
        BWAPI::Unit unit = BWAPI::Broodwar->getUnit(unitId);
        if (unit) {
            BWAPI::Broodwar << "Found Unit: %d " << unitId << std::endl;
            BWAPI::Broodwar->setScreenPosition(unit->getPosition());
        }
        return;
    }
    BWAPI::Unitset selectedUnits = BWAPI::Broodwar->getSelectedUnits();
    if (text == "isStuck") {
        for (BWAPI::Unit unit: selectedUnits) {
            BWAPI::Broodwar->sendTextEx(true, "%d: %s",
                unit->getID(), unit->isStuck() ? "True" : "False");
            }
    }
    else if (text == "getPosition") {
        for (BWAPI::Unit unit: selectedUnits) {
            BWAPI::Position Pos = unit->getPosition();
            BWAPI::Broodwar->sendTextEx(true, "%d: (%d, %d)",
                unit->getID(), Pos.x, Pos.y);
        }
    }
    else if (text == "getLocation") {
        for (BWAPI::Unit unit: selectedUnits) {
            BWAPI::TilePosition Pos = unit->getTilePosition();
            BWAPI::Broodwar->sendTextEx(true, "%d: (%d, %d)",
                unit->getID(), Pos.x, Pos.y);
        }
    }
    else if (text == "getTargetPosition") {
        for (BWAPI::Unit unit: selectedUnits) {
            BWAPI::Position TP = unit->getTargetPosition();
            BWAPI::Broodwar->sendTextEx(true, "%d: (%d, %d)",
                unit->getID(), TP.x, TP.y);
        }
    }
    else if (text == "canAttack") {
        for (BWAPI::Unit unit: selectedUnits) {
            BWAPI::Position TP = unit->getTargetPosition();
            BWAPI::Broodwar->sendTextEx(true,
                unit->getType().groundWeapon() != BWAPI::WeaponTypes::None
                    ? "true" : "false");
        }
    }
    else {
        BWAPI::Broodwar->sendTextEx(true, "'%s'", text.c_str());
    }
}

void GW::onReceiveText(BWAPI::Player player, std::string text)
{
}

void GW::onSaveGame(std::string gameName)
{
}

void GW::onPlayerLeft(BWAPI::Player Player)
{
    cartographer.removePlayerLocations(Player);
}

void GW::onEnd(bool IsWinner)
{
}

void GW::manageProduction()
{
    if (availableSupply <= workerBuffer + armyBuffer &&
        SELF->supplyTotal() != 400)
    {
        if (SUPPLY_TYPE.isBuilding()) {
            // Constructs a pylon or supply depot.
            buildingConstructer.constructUnit(SUPPLY_TYPE);
        }
        else {
            // Trains a overloard.
            // TRAINING[SUPPLY_TYPE].produceSingleUnit(SUPPLY_TYPE);
        }
    }
    else {
        try {
            ecoBaseManager.produceUnits(WORKER_TYPE);
        }
        catch (char* err) {
            BWAPI::Broodwar->sendText(err);
        }
        warriorTrainer.produceUnits(ARMY_UNIT_TYPE);
    }
}

void GW::manageBases()
{
    const int centerPrice = CENTER_TYPE.mineralPrice(),
              armyFacilityPrice = ARMY_ENABLING_TECH_TYPE.mineralPrice();
    if (!PENDING_UNIT_TYPE_COUNT[CENTER_TYPE] &&
        warriorTrainer.facilityCount() >= 2 &&
        ecoBaseManager.isAtCapacity())
    {
        buildingConstructer.constructExpansion(CENTER_TYPE);
    }
    else if (supplyCount && (SELF->minerals() > armyFacilityPrice * 1.5 ||
            !warriorTrainer.isAvailable()))
    {
        // Construct multiple Gateways and Barracks.
        if (ARMY_ENABLING_TECH_TYPE.canProduce())
        {
            buildingConstructer.constructUnit(ARMY_ENABLING_TECH_TYPE);
        }
        // Instead of multiple spawning pools build hatcharies.
        else if (warriorTrainer.isAvailable() ||
                PENDING_UNIT_TYPE_COUNT[ARMY_ENABLING_TECH_TYPE])
        {
            buildingConstructer.constructUnit(BWAPI::UnitTypes::Zerg_Hatchery);
        }
        // This is where we build the spawning pool.
        else
        {
            buildingConstructer.constructUnit(ARMY_ENABLING_TECH_TYPE);
        }
    }
}

void GW::manageAttackGroups()
{
    // ToDo: Change defense to around given positons.
    if (BASE_CENTER->getClosestUnit(IsEnemy, 900))
        squadCommander.assembleSquad();  // Enemy is inside base assemble defenders.
    squadCommander.uniteSquads();
    squadCommander.removeEmptySquads();
}

std::vector<PositionedUnits> GW::getMapMinerals()
{
    // Group static minerals into "Starcraft" defined groups.
    std::map<int, BWAPI::Unitset> groupedMinerals;
    for (BWAPI::Unit Mineral: BWAPI::Broodwar->getStaticMinerals())
        groupedMinerals[Mineral->getResourceGroup()].insert(Mineral);
    std::vector<PositionedUnits> mapMinerals;
    for (auto mineralGroup: groupedMinerals) {
        BWAPI::Unitset mineralCluster = mineralGroup.second;
        // Ignore mineral clusters possibly used as terrain.
        if (mineralCluster.size() > 4)
            mapMinerals.push_back(std::make_pair(
                mineralCluster.getPosition(), mineralCluster));
    }
    return mapMinerals;
}

int GW::getAvailableSupply()
{
    const int supply = SUPPLY_TYPE.supplyProvided(),
              centerSupply = CENTER_TYPE.supplyProvided();
    int supplyConstructing = (PENDING_UNIT_TYPE_COUNT[SUPPLY_TYPE] * supply +
        PENDING_UNIT_TYPE_COUNT[CENTER_TYPE] * centerSupply);
    return SELF->supplyTotal() + supplyConstructing - SELF->supplyUsed();
}

int GW::getUnitBuffer(BWAPI::UnitType unitType)
{
    // Preventing repetitive conversion into a float by using a float.
    const float supplyBuildTime = SUPPLY_TYPE.buildTime();
    int unitSupply = unitType.supplyRequired(),
        unitBuildTime = unitType.buildTime(),
        facilityAmount = (unitType == WORKER_TYPE
            ? ecoBaseManager.getBaseAmount()
            : warriorTrainer.facilityCount()),
        unitsDuringBuild = facilityAmount * std::ceil(
            supplyBuildTime / unitBuildTime);
    return unitsDuringBuild * unitSupply;
}

void GW::scout(std::set<BWAPI::TilePosition> scoutLocations)
{
    // ToDo: Scout with fewer workers.
    // Bug: Won't scout usually Protoss.
    BWAPI::Unitset workerUnits = BASE_CENTER->getUnitsInRadius(
        900, IsWorker && IsOwned && !IsConstructing);
    auto workerIt = workerUnits.begin();
    for (BWAPI::TilePosition Location: scoutLocations) {
        // ToDo: Gather the previous mineral instead.
        BWAPI::Unit Scout = *workerIt++,
                    closestMineral = Scout->getClosestUnit(IsMineralField);
        if (!Scout->move(BWAPI::Position(Location))) {
            cmdRescuer.append(CmdRescuer::MoveCommand(
                Scout, BWAPI::Position(Location), true));
        }
        else if (!Scout->gather(closestMineral, true)) {
            cmdRescuer.append(CmdRescuer::GatherCommand(
                Scout, closestMineral, true));
        }
    }
}

void GW::displayUnitInfo()
{
    int row = 15;
    for (BWAPI::Unit unit: BWAPI::Broodwar->getSelectedUnits()) {
        int sinceCommandFrame = (BWAPI::Broodwar->getFrameCount() -
            unit->getLastCommandFrame());
        BWAPI::UnitCommand lastCmd = unit->getLastCommand();
        BWAPI::Broodwar->drawTextScreen(440, row,
            "%s: %d - %s", unit->getType().c_str(), unit->getID(),
            unit->getOrder().c_str());
        BWAPI::Broodwar->drawTextScreen(440, row + 10,
            "    %d - %s", sinceCommandFrame,
            lastCmd.getType().c_str());
        BWAPI::Unit targetedUnit = unit->getTarget();
        if (targetedUnit) {
            BWAPI::Broodwar->drawLineMap(unit->getPosition(),
                targetedUnit->getPosition(), BWAPI::Color(0, 255, 0));
        }
        if (lastCmd.getType() == BWAPI::UnitCommandTypes::Attack_Unit) {
            BWAPI::Broodwar->registerEvent(
                [unit, lastCmd](BWAPI::Game*)
                    {
                        BWAPI::Broodwar->drawLineMap(unit->getPosition(),
                            lastCmd.getTarget()->getPosition(),
                            BWAPI::Color(255, 0, 0));
                    },
                nullptr, 1);
        }
        row += 25;
    }
}

void GW::displayStatus()
{
    // May perhaps reveal logic errors and bugs.
    BWAPI::Broodwar->drawTextScreen(3, 3, "APM %d, FPS %d, avgFPS %f",
        BWAPI::Broodwar->getAPM(), BWAPI::Broodwar->getFPS(),
        BWAPI::Broodwar->getAverageFPS());
    BWAPI::Broodwar->drawTextScreen(3, 15,
        "availableSupply: %d, Buffer: %d, pendingSupply %d ", availableSupply,
        workerBuffer + armyBuffer, PENDING_UNIT_TYPE_COUNT[SUPPLY_TYPE]);
    int row = 30;
    ecoBaseManager.displayStatus(row);
    warriorTrainer.displayStatus(row);
    buildingConstructer.displayStatus(row);
    squadCommander.displayStatus(row);
    cartographer.displayStatus(row);
    displayUnitInfo();
}

#endif
