#ifndef CMDRESCUER_CPP
#define CMDRESCUER_CPP
#include "CmdRescuer.h"

void CmdRescuer::Rescuer::rescue()
{
    // Curious if this can be done without the lambda.
    Commands.erase(remove_if(
        Commands.begin(), Commands.end(), [](Command Cmd) {
            Cmd.displayMsg();
            return Cmd.execute(); }),
        Commands.end());
}

CmdRescuer::BuildCommand::BuildCommand(
    BWAPI::Unit contractor,
    BWAPI::UnitType constructable,
    BWAPI::TilePosition location)
{
    this->contractor = contractor;
    this->constructable = constructable;
    this->location = location;
}

void CmdRescuer::BuildCommand::displayMsg()
{
    BWAPI::Broodwar << "Recall " << contractor->getID() << " build "
                    << constructable.c_str() << std::endl;
}

CmdRescuer::TrainCommand::TrainCommand(
    BWAPI::Unit trainer,
    BWAPI::UnitType trainee)
{
    this->trainer = trainer;
    this->trainee = trainee;
}

void CmdRescuer::TrainCommand::displayMsg()
{
    BWAPI::Broodwar << "Recall " << trainer->getID() << " train " <<
                       trainee.c_str() << std::endl;
}

CmdRescuer::MoveCommand::MoveCommand(
    BWAPI::Unit runner, BWAPI::Position toThere, bool queue)
{
    this->runner = runner;
    this->toThere = toThere;
    this->queue = queue;
}

void CmdRescuer::MoveCommand::displayMsg()
{
    BWAPI::Broodwar << "Recall " << runner->getID() << " move " << std::endl;
}

CmdRescuer::GatherCommand::GatherCommand(
    BWAPI::Unit miner, BWAPI::Unit mineral, bool queue)
{
    this->miner = miner;
    this->mineral = mineral;
    this->queue = queue;
}

void CmdRescuer::GatherCommand::displayMsg()
{
    BWAPI::Broodwar << "Recall " << miner->getID() << " mine " << std::endl;
}

#endif
