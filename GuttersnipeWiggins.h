#pragma once
#include <BWAPI.h>

class GW : public BWAPI::AIModule
{
public:
    virtual void onStart();
    virtual void onFrame();
    virtual void onUnitCreate(BWAPI::Unit unit);
    virtual void onUnitMorph(BWAPI::Unit unit);
    virtual void onUnitComplete(BWAPI::Unit unit);
    virtual void onUnitDestroy(BWAPI::Unit unit);
    virtual void onUnitDiscover(BWAPI::Unit unit);
    virtual void onUnitEvade(BWAPI::Unit unit);
    virtual void onUnitShow(BWAPI::Unit unit);
    virtual void onUnitHide(BWAPI::Unit unit);
    virtual void onUnitRenegade(BWAPI::Unit unit);
    virtual void onNukeDetect(BWAPI::Position target);
    virtual void onSendText(std::string text);
    virtual void onReceiveText(BWAPI::Player player, std::string text);
    virtual void onSaveGame(std::string gameName);
    virtual void onPlayerLeft(BWAPI::Player player);
    virtual void onEnd(bool isWinner);

private:
    std::set<BWAPI::TilePosition> collectScoutingLocations();
    std::vector<BWAPI::TilePosition> getMineralClusterLocations();
    int getAvailableSupply();
    int getUnitBuffer(BWAPI::UnitType);
    void scout();
    void attack_from(BWAPI::Position);
    void removeLocation(BWAPI::TilePosition Location);
    void removeLocation(BWAPI::Player Player, BWAPI::TilePosition Location);
    int getContractorTask(BWAPI::Unit contractorUnit);
    void constructUnit(BWAPI::UnitType constructableType);
    void constructUnit(
        BWAPI::UnitType constructableType,
        BWAPI::TilePosition constructionLocation,
        BWAPI::Unit contractorUnit);
    BWAPI::TilePosition getExpansionLocation(BWAPI::Unit centerContractor);
    void constructExpansion();
    void displayState();
};
