#pragma once
#include "SquadCommander.h"

using namespace BWAPI::Filter;

void SquadCommander::sendUnitToAttack(
    const BWAPI::Unit& unit, const BWAPI::Position& attackPos)
{
    deployedForces.push_back(Squad(unit, attackPos));
}

void SquadCommander::removeFromDuty(const BWAPI::Unit& deadArmyUnit) {
    for (Squad& squad: deployedForces)
        squad.remove(deadArmyUnit);
};

void SquadCommander::updateGrouping() {
    uniteNearBySquads();
    removeEmptySquads();
}

void SquadCommander::updateTargeting() {
    for (Squad& squad: deployedForces)
        squad.aquireTargets();
}

void SquadCommander::updateAttacking() {
    for (const auto& squad: deployedForces)
        squad.attack();
}

void SquadCommander::uniteNearBySquads() {
    // Preferring iteration by index to prevent pointers of pointers
    if (deployedForces.size() < 2) return;
    int forcesLength = deployedForces.size();
    for (int i = 0; i < forcesLength - 1; ++ i) {
        for (int j = i + 1; j < forcesLength; ++j) {
            if (deployedForces[i].isJoinable(deployedForces[j]))
                deployedForces[i].join(deployedForces[j]);
        }
    }
}

void SquadCommander::removeEmptySquads() {
    deployedForces.erase(
        std::remove_if(deployedForces.begin(), deployedForces.end(), 
            [](Squad squad) { return squad.isEmpty(); }),
        deployedForces.end());
}

void SquadCommander::commandSquadsTo(const BWAPI::Position& attackPosition) {
    for (Squad& squad: deployedForces)
        squad.aggresivePosition = attackPosition;
}

std::vector<BWAPI::Position*> SquadCommander::completed() {
    std::vector<BWAPI::Position*> completedPositions;
    for (Squad& squad: deployedForces) {
        if (squad.completedAttack())
            completedPositions.push_back(&squad.aggresivePosition);
    }
    return completedPositions;
}

/*
std::vector<BWAPI::Position> SquadCommander::getSquadAttackPositions() const {
    std::vector<BWAPI::Position> attackPositions;
    for (Squad& squad: deployedForces)
        attackPositions.push_back(squad.aggresivePosition);
    return attackPositions;
}
*/

Squad::Squad(const BWAPI::Unit& unit, const BWAPI::Position& attackPosition) {
    members.insert(unit) ;
    aggresivePosition = attackPosition;
}

BWAPI::Position Squad::getAvgPosition() const {
    return members.getPosition();
}

void Squad::assign(const BWAPI::Unit& armyUnit) {
    members.insert(armyUnit);
}

void Squad::remove(const BWAPI::Unit& armyUnit) {
    members.erase(armyUnit);
}

bool Squad::isJoinable(const Squad& otherSquad) const {
    if (aggresivePosition != otherSquad.aggresivePosition) return false;
    return (members.getPosition().getApproxDistance(
        otherSquad.members.getPosition()) < 250);
}

void Squad::join(Squad& otherSquad) {
    members.insert(otherSquad.members.begin(), otherSquad.members.end());
    otherSquad.members.clear();
}

void Squad::aquireTargets() {
    if (isUnderAttack())
        aquireAggresiveTargets();
    else
        aquireNormalTargets();
}

bool Squad::isUnderAttack() const {
    for (const BWAPI::Unit& squadMember: members) {
        if (squadMember->isUnderAttack())
            return true;
    }
    return false;
}

void Squad::aquireAggresiveTargets() {
    targets.setTargets(members.getUnitsInRadius(300, IsEnemy && IsDetected &&
        !IsFlying && GroundWeapon != BWAPI::WeaponTypes::None));
}

void Squad::aquireNormalTargets() {
    targets.setTargets(members.getUnitsInRadius(300, IsEnemy && IsDetected &&
        !IsFlying && GetType != BWAPI::UnitTypes::Zerg_Larva));
}

void Squad::attack() const {
    if (targets.empty())
        attackPosition();
    else
        attackTargets();
}

void Squad::attackPosition() const {
    for (const BWAPI::Unit& squadMember: members) {
        if (!isAttackingPosition(squadMember))
            squadMember->move(aggresivePosition);
    }
}

bool Squad::isAttackingPosition(
    const BWAPI::Unit& squadMember) const
{
    const auto& lastCmd = squadMember->getLastCommand();
    return ((lastCmd.getType() == BWAPI::UnitCommandTypes::Move &&
        squadMember->getTargetPosition() == aggresivePosition));
}

void Squad::attackTargets() const {
    for (const BWAPI::Unit& squadMember: members) {
        if (squadMember->isAttackFrame()) continue;
        if (!isAttackingTarget(squadMember))
            moveToClosestTarget(squadMember);
    }
}

void Squad::moveToClosestTarget(
    const BWAPI::Unit& squadMember) const
{
    squadMember->move(getClosestTarget(squadMember)->getPosition());
}

BWAPI::Unit Squad::getClosestTarget(
    const BWAPI::Unit& squadMember) const
{
    const BWAPI::Position& squadPos = squadMember->getPosition();
    BWAPI::Unit closestTarget = *targets.begin();
    int closestDist = squadPos.getApproxDistance(closestTarget->getPosition());
    for (const BWAPI::Unit& enemyTarget: targets) {
        int dist = squadPos.getApproxDistance(enemyTarget->getPosition());
        if (dist < closestDist) {
            closestTarget = enemyTarget;
            closestDist = dist;
        }
    }
    return closestTarget;
}

bool Squad::isAttackingTarget(
    const BWAPI::Unit& squadMember) const
{
    for (const BWAPI::Unit& enemyTarget: targets) {
        if (memberIsTargeting(squadMember, enemyTarget))
            return true;
        if (squadMember->isInWeaponRange(enemyTarget))
            return squadMember->attack(enemyTarget);
    }
    return false;
}

bool Squad::memberIsTargeting(
    const BWAPI::Unit& squadMember, const BWAPI::Unit& target) const
{
    const auto& lastCmd = squadMember->getLastCommand();
    return (lastCmd.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
        lastCmd.getTarget() == target);
}

bool Squad::completedAttack() const {
    return (targets.empty() &&
        aggresivePosition.getApproxDistance(members.getPosition()) < 150);
}

void TargetPrioritizer::setTargets(
    const BWAPI::Unitset& targets)
{
    avgPosition = targets.getPosition();
    enemyUnits.clear();
    enemyUnits = std::vector<BWAPI::Unit>(targets.begin(), targets.end());
    std::sort(enemyUnits.begin(), enemyUnits.end(), greaterPriority);
}

bool TargetPrioritizer::greaterPriority(
    const BWAPI::Unit& unit1, const BWAPI::Unit& unit2)
{
    const BWAPI::UnitType& unit1Type = unit1->getType();
    const BWAPI::UnitType& unit2Type = unit2->getType();
    if (byType(unit1Type) != byType(unit2Type))
        return byType(unit1Type) > byType(unit2Type);
    if (byDamage(unit1Type) != byDamage(unit2Type))
        return byDamage(unit1Type) > byDamage(unit2Type);
    return byDurability(unit1) < byDurability(unit2);
}

int TargetPrioritizer::byType(
    const BWAPI::UnitType& unitType)
{
    if (unitType == BWAPI::UnitTypes::Protoss_Shuttle ||
        unitType == BWAPI::UnitTypes::Terran_Dropship)
    {
        return 5;
    }
    if (unitType == BWAPI::UnitTypes::Protoss_High_Templar ||
        unitType == BWAPI::UnitTypes::Protoss_Dark_Archon ||
        unitType == BWAPI::UnitTypes::Protoss_Reaver ||
        unitType == BWAPI::UnitTypes::Protoss_Carrier ||
        unitType == BWAPI::UnitTypes::Protoss_Arbiter ||
        unitType == BWAPI::UnitTypes::Terran_Medic ||
        unitType == BWAPI::UnitTypes::Terran_Science_Vessel ||
        unitType == BWAPI::UnitTypes::Terran_Ghost ||
        unitType == BWAPI::UnitTypes::Zerg_Queen ||
        unitType == BWAPI::UnitTypes::Zerg_Lurker ||
        unitType == BWAPI::UnitTypes::Zerg_Defiler)
    {
        return 4;
    }
    if (hasWeapon(unitType) && !unitType.isWorker())
        return 3;
    if (unitType == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
        unitType == BWAPI::UnitTypes::Terran_Bunker ||
        unitType == BWAPI::UnitTypes::Zerg_Sunken_Colony)
    {
        return 2;
    }
    if (unitType.isWorker())
        return 1;
    return 0;
}

bool TargetPrioritizer::hasWeapon(
    const BWAPI::UnitType& unitType)
{
    return (unitType.groundWeapon() != BWAPI::WeaponTypes::None ||
            unitType.airWeapon() != BWAPI::WeaponTypes::None);
}

int TargetPrioritizer::byDamage(
    const BWAPI::UnitType& unitType)
{
    BWAPI::WeaponType unitWeapon = unitType.groundWeapon();
    return int(unitWeapon.damageAmount() * unitWeapon.damageFactor() *
        (unitWeapon.damageType() == BWAPI::DamageTypes::Normal ? 1 : 0.65));
}

int TargetPrioritizer::byDurability(
    const BWAPI::Unit& unit)
{
    return  (unit->getShields() + unit->getHitPoints() +
        unit->getType().armor() * 7);
}
