#pragma once
#include <BWAPI.h>
#include "Utils.h"


class SquadCommander {
    private:
        class Squad;
        std::vector<Squad> deployedForces;
        std::vector<BWAPI::Position> attackSeries;
        BWAPI::Position safePosition;
        void uniteNearBySquads();
        void removeEmptySquads();
    public:
        void addAttackPosition(const BWAPI::Position& attackPosition);
        void setAttackSeries(const std::vector<BWAPI::Position>&);
        void setSafePosition(const BWAPI::Position& safePosition);
        void enlistForDeployment(const BWAPI::Unit& units);
        void removeFromDuty(const BWAPI::Unit& armyUnit);
        void updateGrouping();
        void updateTargeting();
        void updateAttacking();
};

class SquadCommander::Squad {
    private:
        BWAPI::Unitset members;
        class TargetPrioritizer {
            private:
                std::vector<BWAPI::Unit> enemyUnits;
                BWAPI::Position avgPosition;
                static bool greaterPriority(
                    const BWAPI::Unit&, const BWAPI::Unit&);
                static int byType(const BWAPI::UnitType& unitType);
                static bool hasWeapon(const BWAPI::UnitType& unitType);
                static int byDamage(const BWAPI::UnitType& unitType);
                static int byDurability(const BWAPI::Unit& unit);
            public:
                BWAPI::Position getAvgPosition() const { return avgPosition; }
                void setTargets(const BWAPI::Unitset& targets);
                std::vector<BWAPI::Unit>::const_iterator begin() const
                    { return enemyUnits.cbegin(); }
                std::vector<BWAPI::Unit>::const_iterator end() const
                    { return enemyUnits.cend(); }
                bool isEmpty() const { return enemyUnits.empty(); }
        } targets;
        bool isUnderAttack() const;
        void aquireAggresiveTargets();
        void aquireNormalTargets();
        void attackPosition() const;
        bool isAttackingPosition(const BWAPI::Unit& squadMember) const;
        void attackTargets() const;
        void moveToClosestTarget(const BWAPI::Unit& squadMember) const;
        BWAPI::Unit getClosestTarget(const BWAPI::Unit& squadMember) const;
        bool isAttackingTarget(const BWAPI::Unit& squadMember) const;
        bool memberIsTargeting(const BWAPI::Unit&, const BWAPI::Unit&) const;
    public:
        Squad(const BWAPI::Unit&, const BWAPI::Position&,
            const BWAPI::Position&);
        BWAPI::Position aggresivePosition, defensivePosition;
        BWAPI::Position getAvgPosition() const;
        bool isEmpty() const { return members.empty(); };
        void assign(const BWAPI::Unit& armyUnit);
        void remove(const BWAPI::Unit& deadArmyUnit);
        bool isJoinable(const Squad& otherSquad) const;
        void join(Squad& otherSquad);
        // void split(const int& newSquadAmount);
        void aquireTargets();
        void attack() const;
};
