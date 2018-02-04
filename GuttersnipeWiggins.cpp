#ifndef GUTTERSNIPEWIGGINS_CPP
#define GUTTERSNIPEWIGGINS_CPP
#include "GuttersnipeWiggins.h"

using namespace BWAPI::Filter;

void GW::onStart()
{
    BWAPI::Broodwar->enableFlag(1);  // Enabled for debugging.
    Self = BWAPI::Broodwar->self();
    cartographer.discoverResourcePositions();
    squadCommander.setAttackSeries(
        cartographer.getUnexploredStartingPositions());
    squadCommander.setSafePosition(BWAPI::Position(Self->getStartLocation()));
    switch (Self->getRace()) {
        case BWAPI::Races::Enum::Protoss:
            race = new ProtossRace();
            break;
        case BWAPI::Races::Enum::Terran:
            race = new TerranRace();
            break;
        case BWAPI::Races::Enum::Zerg:
            race = new ZergRace();
            break;
    }
    decisionSequence.onStart(race);
}

void GW::onFrame()
{
    const int actionFrames = std::max(7, BWAPI::Broodwar->getLatency());
    GW::displayStatus();  // For debugging.
    switch(BWAPI::Broodwar->getFrameCount() % actionFrames) {
        case 0: decisionSequence.update();
            break;
        case 1: race->update();
            break;
        case 2: cartographer.update();
            break;
        case 3: squadCommander.updateGrouping();
            break;
        case 4: squadCommander.updateTargeting();
            break;
        case 5: squadCommander.updateAttacking();
            break;
        case 6:
            cmdRescuer.rescue();
            break;
        default: break;
    }
}

void GW::onUnitCreate(BWAPI::Unit Unit)
{
    if (Unit->getPlayer() == Self) {
        race->onUnitCreate(Unit);
    }
}

void GW::onUnitMorph(BWAPI::Unit Unit)
{
    if (Unit->getPlayer() == Self)
        race->onUnitMorph(Unit);
}

void GW::onUnitComplete(BWAPI::Unit Unit)
{
    if (Unit->getPlayer() == Self) {
        race->onUnitComplete(Unit);
        if (Unit->getType() == race->getArmyUnitType())
            squadCommander.enlistForDeployment(Unit);
    }
}

void GW::onUnitDestroy(BWAPI::Unit Unit)
{
    if (Unit->getPlayer() == Self)
        race->onUnitDestroy(Unit);
    else
        cartographer.removeUnit(Unit);
}

void GW::onUnitDiscover(BWAPI::Unit Unit)
{
    if (Unit->getPlayer() != Self && !Unit->getPlayer()->isNeutral())
        cartographer.addUnit(Unit);
}

void GW::onUnitEvade(BWAPI::Unit Unit) {
    // I don't understand the purpose/use when given the unit is only
    // accessable if it were ours. As such it is a poor duplicate of
    // onUnitDestroyed. Cartographer->update() preforms this function.
}

void GW::onUnitShow(BWAPI::Unit Unit)
{
}

void GW::onUnitHide(BWAPI::Unit Unit)
{
}

void GW::onUnitRenegade(BWAPI::Unit Unit)
{
}

void GW::onNukeDetect(BWAPI::Position target)
{
}

void GW::onSendText(std::string text)
{
    if (text == "getError") {
        BWAPI::Broodwar << BWAPI::Broodwar->getLastError() << std::endl;
        return;
    }
    else if (text.substr(0, 7) == "getUnit") {
        int unitId = atoi(text.substr(8, 4).c_str());
        BWAPI::Unit unit = BWAPI::Broodwar->getUnit(unitId);
        if (unit) {
            BWAPI::Broodwar << "Found Unit: %d " << unitId << std::endl;
            BWAPI::Broodwar->setScreenPosition(unit->getPosition());
        }
        return;
    }
    // else if (text.substr(0, 6) == "getJob") {
        // int unitId = atoi(text.substr(8, 4).c_str());
        // BWAPI::Unit unit = BWAPI::Broodwar->getUnit(unitId);
    // }
    BWAPI::Unitset selectedUnits = BWAPI::Broodwar->getSelectedUnits();
    if (text == "isStuck") {
        for (BWAPI::Unit unit: selectedUnits) {
            BWAPI::Broodwar->sendTextEx(true, "%d: %s",
                unit->getID(), unit->isStuck() ? "True" : "False");
            }
    }
    else if (text == "getPos") {
        for (BWAPI::Unit unit: selectedUnits) {
            BWAPI::Position Pos = unit->getPosition();
            BWAPI::Broodwar->sendTextEx(true, "%d: (%d, %d)",
                unit->getID(), Pos.x, Pos.y);
        }
    }
    else if (text == "getLoc") {
        for (BWAPI::Unit unit: selectedUnits) {
            BWAPI::TilePosition Pos = unit->getTilePosition();
            BWAPI::Broodwar->sendTextEx(true, "%d: (%d, %d)",
                unit->getID(), Pos.x, Pos.y);
        }
    }
    else if (text == "getTargetPos") {
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
}

void GW::onEnd(bool IsWinner)
{
    delete race;
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
    displayUnitInfo();
}

#endif
