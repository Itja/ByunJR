#pragma once
#include <sc2api/sc2_api.h>

#include "information/UnitInfoManager.h"
#include "macro/BuildingPlacer.h"

class ByunJRBot;

// InformationManager is not inside the information folder in order to remind people not to include individual things from the information folder.
// Only include InformationManager, and use it to get and set specific information about the game.
class InformationManager
{
    ByunJRBot &              bot_;
    BuildingPlacer           building_placer_;
    UnitInfoManager          unit_info_;

    sc2::Race                player_race_[2];
    vvi                      dps_map_;

public:
    InformationManager(ByunJRBot & bot);
    void OnStart();
    void OnUnitCreated(const sc2::Unit* unit);
    void OnUnitDestroyed(const sc2::Unit* unit);
    void OnFrame();
    BuildingPlacer & BuildingPlacer();
    UnitInfoManager & UnitInfo();

    sc2::Point2DI GetProxyLocation() const;

    const sc2::Unit* GetBuilder(Building& b, bool set_job_as_builder = true);
    const sc2::Race & GetPlayerRace(PlayerArrayIndex player) const;

    const sc2::Unit* GetClosestBase(const sc2::Unit* reference_unit) const;
    const ::UnitInfo* GetClosestUnitInfoWithJob(const sc2::Point2D point, const UnitMission) const;
    const sc2::Unit* GetClosestUnitWithJob(const sc2::Point2D reference_point, const UnitMission unit_mission) const;
    const ::UnitInfo* GetClosestUnitInfoWithJob(const sc2::Point2D point, const std::vector<UnitMission> mission) const;
    const sc2::Unit* GetClosestUnitWithJob(const sc2::Point2D point,
                                            const std::vector<UnitMission> mission_vector) const;
    const sc2::Unit* GetClosestUnitOfType(const sc2::Unit* unit, const sc2::UnitTypeID) const;
    const sc2::Unit* GetClosestNotOptimalRefinery(const sc2::Unit* reference_unit) const;
    vvi GetDPSMap() const;
};
