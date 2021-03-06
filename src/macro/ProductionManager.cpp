#include <sstream>

#include "ByunJRBot.h"
#include "common/Common.h"
#include "macro/ProductionManager.h"
#include "micro/Micro.h"
#include "util/Util.h"

ProductionManager::ProductionManager(ByunJRBot & bot)
    : bot_             (bot)
    , building_manager_ (bot)
    , queue_           (bot)
{

}

void ProductionManager::OnStart()
{
    planned_supply_depots_ = 0;
    building_manager_.OnStart();
    SetBuildOrder(bot_.Strategy().GetOpeningBookBuildOrder());
}

void ProductionManager::OnFrame()
{
    // Dynamically spend our money based on our current needs. 
    PreventSupplyBlock();
    MacroUp();

    // check the _queue for stuff we can build
    ManageBuildOrderQueue();

    // TODO: if nothing is currently building, get a new goal from the strategy manager
    // TODO: detect if there's a build order deadlock once per second
    // TODO: triggers for game things like cloaked units etc

    building_manager_.OnFrame();
    DrawProductionInformation();
}

void ProductionManager::OnBuildingConstructionComplete(const sc2::Unit* unit) {
    if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT)
    {
        planned_supply_depots_--;
    }
}

void ProductionManager::OnUnitDestroyed(const sc2::Unit* building)
{
    // The building is dead! We can build where it used to be!
    if(Util::IsBuilding(building->unit_type))
        bot_.InformationManager().BuildingPlacer().FreeTiles(building->unit_type, sc2::Point2DI(building->pos.x, building->pos.y));
}

void ProductionManager::SetBuildOrder(const BuildOrder & build_order)
{
    queue_.ClearAll();

    for (size_t i(0); i<build_order.Size(); ++i)
    {
        queue_.QueueAsLowestPriority(build_order[i], true);
    }
}

// Called every frame.
void ProductionManager::ManageBuildOrderQueue()
{
    // if there is nothing in the queue, oh well
    if (queue_.IsEmpty())
    {
        return;
    }

    // the current item to be used
    BuildOrderItem & current_item = queue_.GetHighestPriorityItem();

    // while there is still something left in the queue
    while (!queue_.IsEmpty())
    {
        // this is the unit which can produce the currentItem
        const sc2::Unit* producer = GetProducer(current_item.type);

        // check to see if we can make it right now
        const bool can_make = CanMakeNow(producer, current_item.type);

        // TODO: if it's a building and we can't make it yet, predict the worker movement to the location

        // if we can make the current item
        if (producer && can_make)
        {
            // create it and remove it from the _queue
            Create(producer, current_item);
            queue_.RemoveCurrentHighestPriorityItem();

            // don't actually loop around in here
            break;
        }
        // otherwise, if we can skip the current item
        else if (queue_.CanSkipItem())
        {
            // skip it
            queue_.SkipItem();

            // and get the next one
            current_item = queue_.GetNextHighestPriorityItem();
        }
        else
        {
            // so break out
            break;
        }
    }
}

size_t ProductionManager::NumberOfUnitsInProductionOfType(sc2::UnitTypeID unit_type) const
{
    return building_manager_.NumberOfUnitsInProductionOfType(unit_type);
}

bool has_completed_wall = false;
// Every frame, see if more depots are required. 
void ProductionManager::PreventSupplyBlock() {
    // If the current supply that we have plus the total amount of things that could be made 
    if ( 
        // If we are at max supply, there is no point in building more depots. 
         bot_.Observation()->GetFoodCap() < 400
        && (bot_.Observation()->GetFoodUsed() + ProductionCapacity())  // We used to compare only against things that are planned on being made
                                                            // Is greater than 
        >=
        // the player supply capacity, including pylons in production. 
        // The depots in production is key, otherwise you will build hundreds of pylons while suppsuly blocked.
        (bot_.Observation()->GetFoodCap() + (planned_supply_depots_ * 8)) // Not sure how to get supply provided by a depot, lets just go with 8.
        )
    {
        planned_supply_depots_++;
        queue_.QueueAsHighestPriority(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT, true);
    }

    // Build wall if needed.
    if (bot_.InformationManager().GetPlayerRace(PlayerArrayIndex::Enemy) == sc2::Zerg
        && Util::GetGameTimeInSeconds(bot_) > 50 && !has_completed_wall)
    {
        has_completed_wall = true;
        queue_.QueueAsHighestPriority(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT, true);
        queue_.QueueAsHighestPriority(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT, true);
    }
}

int ProductionManager::TrueUnitCount(sc2::UnitTypeID unit_type)
{
    return bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, unit_type)
        + queue_.GetItemsInQueueOfType(unit_type)
        + bot_.InformationManager().UnitInfo().UnitsInProductionOfType(unit_type);
}

// Every frame, see if more depots are required. 
void ProductionManager::MacroUp() {
    // Macro up.
    const int scv_count = bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_SCV);
    const int base_count = bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER);
    const int barracks_count = bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKS);
    const int starport_count = bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_STARPORT);

    if (bot_.Strategy().ShouldExpandNow()
        // Don't queue more bases than you have minerals for.
     && queue_.GetItemsInQueueOfType(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER)
        + bot_.InformationManager().UnitInfo().UnitsInProductionOfType(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER) 
        < bot_.Observation()->GetMinerals() / 400)
    {
        queue_.QueueItem(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER, 2);
    }
    if(base_count > 1 && TrueUnitCount(sc2::UNIT_TYPEID::TERRAN_REFINERY) < base_count*2)
    {
        queue_.QueueItem(sc2::UNIT_TYPEID::TERRAN_REFINERY, 2);
    }

    if(bot_.Strategy().MacroGoal() == Strategy::ReaperRush)
    {
        for (const auto & unit : bot_.InformationManager().UnitInfo().GetUnits(PlayerArrayIndex::Self))
        {
            // Constantly make SCV's. At this level of play, no reason not to.
            // Skip one scv to get the proxy barracks up faster. 
            if (Util::IsTownHall(unit) && unit->orders.size() == 0 && (scv_count < 15 || barracks_count > 1) && scv_count < base_count * 23 && scv_count < 80)
            {
                Micro::SmartTrain(unit, sc2::UNIT_TYPEID::TERRAN_SCV, bot_);
            }

            // Get ready to make CattleBruisers
            if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKS && unit->orders.size() == 0)
            {
                Micro::SmartTrain(unit, sc2::UNIT_TYPEID::TERRAN_REAPER, bot_);
                //queue_.QueueItem(sc2::UNIT_TYPEID::TERRAN_REAPER, 5);
            }
        }
    }
    else if(bot_.Strategy().MacroGoal() == Strategy::BattlecruiserMacro)
    {
        for (const auto & unit : bot_.InformationManager().UnitInfo().GetUnits(PlayerArrayIndex::Self))
        {
            // Constantly make SCV's. At this level of play, no reason not to.
            if (Util::IsTownHall(unit) && unit->orders.size() == 0 && scv_count < base_count * 23 && scv_count < 80)
            {
                Micro::SmartTrain(unit, sc2::UNIT_TYPEID::TERRAN_SCV, bot_);
            }

            // Get ready to make CattleBruisers
            if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_STARPORT && unit->orders.size() == 0)
            {
                Micro::SmartTrain(unit, sc2::UNIT_TYPEID::TERRAN_TECHLAB, bot_);
                Micro::SmartTrain(unit, sc2::UNIT_TYPEID::TERRAN_BATTLECRUISER, bot_);
            }
            if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_ARMORY && unit->orders.size() == 0)
            {
                bot_.Actions()->UnitCommand(unit, sc2::ABILITY_ID::RESEARCH_TERRANSHIPWEAPONS);
            }
            if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_ARMORY && unit->orders.size() == 0)
            {
                bot_.Actions()->UnitCommand(unit, sc2::ABILITY_ID::RESEARCH_TERRANVEHICLEANDSHIPPLATING);
            }
            if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_FUSIONCORE && unit->orders.size() == 0)
            {
                bot_.Actions()->UnitCommand(unit, sc2::ABILITY_ID::RESEARCH_BATTLECRUISERWEAPONREFIT);
            }

            if (base_count > 1 && TrueUnitCount(sc2::UNIT_TYPEID::TERRAN_STARPORT) < base_count - 1)
            {
                queue_.QueueItem(sc2::UNIT_TYPEID::TERRAN_STARPORT, 2);
            } 
        }
    }
}

int ProductionManager::ProductionCapacity() const
{
    const  size_t command_centers = bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER)
                                  + bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND)
                                  + bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS);

    const size_t barracks = bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_BARRACKS);
    const size_t factory = bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_FACTORY);
    const size_t starport = bot_.InformationManager().UnitInfo().GetUnitTypeCount(PlayerArrayIndex::Self, sc2::UNIT_TYPEID::TERRAN_STARPORT);
    // Factories and starports can build really supply intensive units. Make sure we have enough supply. 
    return static_cast<int>(command_centers + barracks) * 2 + factory * 4 + starport * 12;
}

const sc2::Unit* ProductionManager::GetProducer(const sc2::UnitTypeID t, const sc2::Point2D closest_to) const
{
    // TODO: get the type of unit that builds this
    const sc2::UnitTypeID producer_type = Util::WhatBuilds(t);

    // make a set of all candidate producers
    std::vector<const sc2::Unit*> candidate_producers;
    for (auto & unit : bot_.InformationManager().UnitInfo().GetUnits(PlayerArrayIndex::Self))
    {
        // reasons a unit can not train the desired type
        if (unit->unit_type != producer_type) { continue; }
        if (unit->build_progress < 1.0f) { continue; }
        if (Util::IsBuilding(producer_type) && unit->orders.size() > 0) { continue; }
        // TODO: if unit is not powered continue
        if (unit->is_flying) { continue; }

        // TODO: if the type is an addon, some special cases
        // TODO: if the type requires an addon and the producer doesn't have one

        // if we haven't cut it, add it to the set of candidates
        candidate_producers.push_back(unit);
    }

    return GetClosestUnitToPosition(candidate_producers, closest_to);
}

const sc2::Unit* ProductionManager::GetClosestUnitToPosition(const std::vector<const sc2::Unit*> & units, const sc2::Point2D closest_to) const
{
    if (units.size() == 0)
    {
        return nullptr;
    }

    // if we don't care where the unit is return the first one we have
    if (closest_to.x == 0 && closest_to.y == 0)
    {
        return units[0];
    }

    const sc2::Unit* closest_unit = nullptr;
    double min_dist = std::numeric_limits<double>::max();

    for (auto & unit : units)
    {
        const double distance = Util::Dist(unit->pos, closest_to);
        if (!closest_unit || distance < min_dist)
        {
            closest_unit = unit;
            min_dist = distance;
        }
    }

    return closest_unit;
}

// this function will check to see if all preconditions are met and then create a unit
void ProductionManager::Create(const sc2::Unit* producer, BuildOrderItem & item)
{
    if (!producer)
    {
        return;
    }

    const sc2::UnitTypeID item_type = item.type;

    // if we're dealing with a building
    // TODO: deal with morphed buildings & addons
    if (Util::IsBuilding(item_type))
    {
        building_manager_.AddBuildingTask(item_type);
    }
    // if we're dealing with a non-building unit
    else
    {
        Micro::SmartTrain(producer, item_type, bot_);
    }
}

bool ProductionManager::CanMakeNow(const sc2::Unit* producer_unit, const sc2::UnitTypeID type) const
{
    if (!MeetsReservedResources(type))
    {
        return false;
    }
    if(producer_unit==nullptr)
        return false;

    sc2::AvailableAbilities available_abilities = bot_.Query()->GetAbilitiesForUnit(producer_unit);

    // quick check if the unit can't do anything it certainly can't build the thing we want
    if (available_abilities.abilities.empty())
    {
        return false;
    }
    else
    {
        // check to see if one of the unit's available abilities matches the build ability type
        const sc2::AbilityID build_type_ability = Util::UnitTypeIDToAbilityID(type);
        for (const sc2::AvailableAbility & available_ability : available_abilities.abilities)
        {
            if (available_ability.ability_id == build_type_ability)
            {
                return true;
            }
        }
    }

    return false;
}

bool ProductionManager::DetectBuildOrderDeadlock() const
{
    // TODO: detect build order deadlocks here
    return false;
}

// Shorthand for getting minerals from the observation layer. 
int ProductionManager::GetFreeMinerals() const
{
    return bot_.Observation()->GetMinerals();
}

// Shorthand for getting gas from the observation layer. 
int ProductionManager::GetFreeGas() const
{
    return bot_.Observation()->GetVespene();
}

// return whether or not we meet resources, including building reserves
bool ProductionManager::MeetsReservedResources(const sc2::UnitTypeID type) const
{
    // return whether or not we meet the resources
    return (Util::GetUnitTypeMineralPrice(type, bot_) <= GetFreeMinerals()) && (Util::GetUnitTypeGasPrice(type, bot_) <= GetFreeGas());
}

void ProductionManager::DrawProductionInformation() const
{
    if (!bot_.Config().DrawProductionInfo)
    {
        return;
    }

    std::stringstream ss;
    ss << "Production Information\n\n";

    for (auto & unit : bot_.InformationManager().UnitInfo().GetUnits(PlayerArrayIndex::Self))
    {
        if (unit->build_progress < 1.0f)
        {
            //ss << sc2::UnitTypeToName(unit->unit_type) << " " << unit->build_progress << std::endl;
        }
    }

    ss << queue_.GetQueueInformation();

    bot_.DebugHelper().DrawTextScreen(sc2::Point2D(0.01f, 0.01f), ss.str(), sc2::Colors::Yellow);
}
