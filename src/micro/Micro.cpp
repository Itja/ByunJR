#include "ByunJRBot.h"
#include "common/Common.h"
#include "micro/Micro.h"
#include "util/Util.h"

const float dotRadius = 0.1f;

void Micro::SmartAttackUnit(const sc2::Tag & attackerTag, const sc2::Tag & targetTag, ByunJRBot & bot)
{
	//UAB_ASSERT(attacker, "SmartAttackUnit: Attacker not valid");
	//UAB_ASSERT(target, "SmartAttackUnit: Target not valid");

	//if (!attacker || !target)
	//{
	//	return;
	//}

	//// if we have issued a command to this unit already this frame, ignore this one
	//if (attacker->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || attacker->isAttackFrame())
	//{
	//	return;
	//}

	//// get the unit's current command
	//BWAPI::UnitCommand currentCommand(attacker->getLastCommand());

	//// if we've already told this unit to attack this target, ignore this command
	//if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&	currentCommand.getTarget() == target)
	//{
	//	return;
	//}

	//// if nothing prevents it, attack the target
	//attacker->attack(target);
	//TotalCommands++;

	//if (Config::Debug::DrawUnitTargetInfo)
	//{
	//	BWAPI::Broodwar->drawCircleMap(attacker->getPosition(), dotRadius, BWAPI::Colors::Red, true);
	//	BWAPI::Broodwar->drawCircleMap(target->getPosition(), dotRadius, BWAPI::Colors::Red, true);
	//	BWAPI::Broodwar->drawLineMap(attacker->getPosition(), target->getPosition(), BWAPI::Colors::Red);
	//}

    // Prevent sending duplicate commands to give an accurate APM measurement in replays
    bool sentCommandAlready = false;
    const sc2::Unit * attacker = bot.GetUnit(attackerTag);
    const sc2::Unit * target = bot.GetUnit(targetTag);
    for (sc2::UnitOrder theOrder : attacker->orders)
    {
        if (theOrder.ability_id == sc2::ABILITY_ID::MOVE && theOrder.target_unit_tag == targetTag)
        {
            sentCommandAlready = true;
        }
    }
    if (sentCommandAlready == false)
        bot.Actions()->UnitCommand(attacker, sc2::ABILITY_ID::ATTACK_ATTACK, target);
}

void Micro::SmartAttackMove(const sc2::Tag & attackerTag, const sc2::Point2D & targetPosition, ByunJRBot & bot)
{
    bot.Actions()->UnitCommand(bot.GetUnit(attackerTag), sc2::ABILITY_ID::ATTACK_ATTACK, targetPosition);
}

void Micro::SmartMove(const sc2::Tag & unitTag, const sc2::Point2D & targetPosition, ByunJRBot & bot)
{
    // Prevent sending duplicate commands to give an accurate APM measurement in replays
    bool sentCommandAlready = false;
    const sc2::Unit * unitToMove = bot.GetUnit(unitTag);
    for (sc2::UnitOrder theOrder : unitToMove->orders)
    {
        if (theOrder.ability_id == sc2::ABILITY_ID::MOVE && theOrder.target_pos == targetPosition)
        {
            sentCommandAlready = true;
        }
    }
    if (sentCommandAlready == false)
        bot.Actions()->UnitCommand(unitToMove, sc2::ABILITY_ID::MOVE, targetPosition);
}

void Micro::SmartRightClick(const sc2::Tag & unitTag, const sc2::Tag & targetTag, ByunJRBot & bot)
{
    bot.Actions()->UnitCommand(bot.GetUnit(unitTag), sc2::ABILITY_ID::SMART, bot.GetUnit(targetTag));
}

void Micro::SmartRepair(const sc2::Tag & unitTag, const sc2::Tag & targetTag, ByunJRBot & bot)
{
    bot.Actions()->UnitCommand(bot.GetUnit(unitTag), sc2::ABILITY_ID::SMART, bot.GetUnit(targetTag));
}

void Micro::SmartKiteTarget(const sc2::Tag & rangedUnitTag, const sc2::Tag & targetTag, ByunJRBot & bot)
{
    //UAB_ASSERT(rangedUnit, "SmartKiteTarget: Unit not valid");
    //UAB_ASSERT(target, "SmartKiteTarget: Target not valid");

    //if (!rangedUnit || !target)
    //{
    //	return;
    //}
    const sc2::Unit * rangedUnit = bot.GetUnit(rangedUnitTag);
    const sc2::Unit * target = bot.GetUnit(targetTag);
    const float range = Util::GetAttackRange(rangedUnit->unit_type, bot);

    //// determine whether the target can be kited
    //bool kiteLonger = Config::Micro::KiteLongerRangedUnits.find(rangedUnit->getType()) != Config::Micro::KiteLongerRangedUnits.end();
    //if (!kiteLonger && (range <= target->getType().groundWeapon().maxRange()))
    //{
    //	// if we can't kite it, there's no point
    //	Micro::SmartAttackUnit(rangedUnit, target);
    //	return;
    //}

    bool kite(true);
    const double dist(bot.Map().getGroundDistance(rangedUnit->pos, target->pos));
    const double speed(bot.Observation()->GetUnitTypeData()[rangedUnit->unit_type].movement_speed);


    // if the unit can't attack back don't kite
    //if (bot.GetUnit(target)->is_flying /*&& !UnitUtil::CanAttackAir(target)) || (!rangedUnit->isFlying() && !UnitUtil::CanAttackGround(target))*/)
    //{
    //	kite = false;
    //}

    const double timeToEnter = (dist - range) / speed;
    // If we start moving back to attack, will our weapon be off cooldown?
    if ((timeToEnter >= rangedUnit->weapon_cooldown))
    {
        kite = false;
    }

    if (Util::IsBuilding(target->unit_type))
    {
        kite = false;
    }

    sc2::Point2D fleePosition;
    if (rangedUnit->health < Util::EnemyDPSInRange(rangedUnit->pos, bot) + 5.0)
    {
        //std::cout << Util::EnemyDPSInRange(rangedUnit->pos, bot) << std::endl;
        kite = true;
        bot.Map().drawBoxAroundUnit(rangedUnitTag, sc2::Colors::Red);
        fleePosition = bot.Bases().getPlayerStartingBaseLocation(PlayerArrayIndex::Self)->getPosition();
    }
    else
    {
        // kite if we are not close to death.
        bot.Map().drawBoxAroundUnit(rangedUnitTag, sc2::Colors::Green);
        fleePosition = rangedUnit->pos - target->pos + rangedUnit->pos;
    }

    //// if we can't shoot, run away
    if (kite)
    {
        //fleePosition = rangedUnit->pos - target->pos + rangedUnit->pos;
        bot.Map().drawLine(rangedUnit->pos, fleePosition);
        Micro::SmartMove(rangedUnit->tag, fleePosition, bot);
    }
    //// otherwise shoot
    else
    {
        //bot.Actions()->UnitCommand(rangedUnit, sc2::ABILITY_ID::EFFECT_KD8CHARGE, target);
        bot.Map().drawLine(rangedUnit->pos, target->pos, sc2::Colors::Red);
        SmartAttackUnit(rangedUnit->tag, target->tag, bot);
    }
}

void Micro::SmartBuild(const sc2::Tag & builderTag, const sc2::UnitTypeID & buildingType, const sc2::Point2D pos, ByunJRBot & bot)
{
    bot.Actions()->UnitCommand(bot.GetUnit(builderTag), Util::UnitTypeIDToAbilityID(buildingType), pos);
}

void Micro::SmartBuildTag(const sc2::Tag & builderTag, const sc2::UnitTypeID & buildingType, const sc2::Tag targetTag, ByunJRBot & bot)
{
    bot.Actions()->UnitCommand(bot.GetUnit(builderTag), Util::UnitTypeIDToAbilityID(buildingType), bot.GetUnit(targetTag));
}

void Micro::SmartTrain(const sc2::Tag & builderTag, const sc2::UnitTypeID & buildingType, ByunJRBot & bot)
{
    bot.Actions()->UnitCommand(bot.GetUnit(builderTag), Util::UnitTypeIDToAbilityID(buildingType));
}