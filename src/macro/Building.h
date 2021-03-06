#pragma once
#include <sc2api/sc2_api.h>

enum class BuildingStatus { Unassigned = 0, Assigned = 1, UnderConstruction = 2, Size = 3 };

struct Building
{
    sc2::Point2DI       finalPosition;
    sc2::Point2D        position;
    sc2::UnitTypeID     type;
    // The pointer to the building.
    const sc2::Unit*    buildingUnit;
    // The pointer to the worker that will make the building.
    const sc2::Unit*    builderUnit;
    BuildingStatus      status;
    int                 lastOrderFrame;
    bool                buildCommandGiven;
    bool                underConstruction;

    // constructor we use most often
    Building(sc2::UnitTypeID t);

    // equals operator
    bool operator == (const Building & b);
};
