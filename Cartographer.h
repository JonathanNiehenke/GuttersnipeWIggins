#ifndef CARTOGRAPHER_H
#define CARTOGRAPHER_H
#include <BWAPI.h>
#include "Utils.h"

class Cartographer
{
    typedef std::pair<BWAPI::Position, BWAPI::Unitset> PositionedUnits;
    typedef std::set<BWAPI::TilePosition> locationSet;
    private:
        int resourceCount = 0;
        std::vector<BWAPI::Unitset> Minerals;
        std::vector<BWAPI::Position> resourcePositons;
        std::map<BWAPI::Player, locationSet> enemyLocations;
        BWAPI::Position startPosition = BWAPI::Positions::Unknown;
    public:
        void discoverResources(BWAPI::Position startPosition);
        int getResourceCount() { return resourceCount; }
        std::vector<BWAPI::Position> getResourcePositions();
        std::vector<BWAPI::Unitset> getMinerals() { return Minerals; }
        void addBuildingLocation(BWAPI::Player, BWAPI::TilePosition);
        void removeBuildingLocation(BWAPI::Player, BWAPI::TilePosition);
        void removeBuildingLocation(BWAPI::TilePosition buildingLocation);
        void removePlayerLocations(BWAPI::Player deadPlayer);
        BWAPI::TilePosition getClosestEnemyLocation(BWAPI::Position);
        locationSet getStartingLocations();
        void displayStatus(int &row);
        BWAPI::Position operator[](int i);
};

#endif

