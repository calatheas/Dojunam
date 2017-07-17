#include "VultureManager.h"
#include "CommandUtil.h"

using namespace MyBot;

VultureManager::VultureManager()
{
	for (BWTA::Chokepoint * chokepoint : BWTA::getChokepoints())
	{
		if (chokepoint == InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self()))
			continue ;
		if (chokepoint == InformationManager::Instance().getFirstChokePoint(BWAPI::Broodwar->enemy()))
			continue;
		chokePointMineCounter[secondChokePointCount] = 0;
		chokePointForVulture[secondChokePointCount] = chokepoint->getCenter();
		secondChokePointCount++;
		//if (chokepoint == InformationManager::Instance().getFirstChokePoint(BWAPI::Broodwar->enemy()))
		//	continue ;
	}
}

void VultureManager::executeMicro(const BWAPI::Unitset & targets)
{
	assignTargetsOld(targets);
}


void VultureManager::assignTargetsOld(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & vultureUnits = getUnits();
	// figure out targets
	BWAPI::Unitset vultureUnitTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(vultureUnitTargets, vultureUnitTargets.end()), [](BWAPI::Unit u){ return u->isVisible(); });	
	for (auto & vultureUnit : vultureUnits)
	{
		// train sub units such as scarabs or interceptors
		//trainSubUnits(vultureUnit);
		//if (order.getType() == SquadOrderTypes::MineInstallation)
		// ¸¸µé±î ¸»±î °í¹ÎµÊ 
		{
			
		}
		//else
		// if the order is to attack or defend
		if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend)
		{
			if (vultureUnitTargets.empty())
			{				
				if (vultureUnit->getSpiderMineCount() > 0)
				{
					int minV = 999999;
					int index = -1;
					BWAPI::Position mineSetPosition = vultureUnit->getPosition();
					//std::cout << "getSpiderMineCount>0  52 done " << std::endl;
					mineSetPosition = chokePointForVulture[(vultureUnit->getID() + vultureUnit->getSpiderMineCount()) % secondChokePointCount];
					//std::cout << " vulture No.  " << vultureUnit->getID() << " set Mine pos ( " << mineSetPosition.x << ", " << mineSetPosition.y << ")" << std::endl;
					BWAPI::Broodwar->drawTextMap(mineSetPosition, "%s", "Mine Set Here");
					//int mineCount = vultureUnit->getSpiderMineCount();
					while (!vultureUnit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, mineSetPosition))
					{						
						if (vultureUnit->getID() % 4 == 0)
							mineSetPosition += BWAPI::Position(1, 1);
						else if (vultureUnit->getID() % 4 == 1)
							mineSetPosition += BWAPI::Position(1, -1);
						else if (vultureUnit->getID() % 4 == 2)
							mineSetPosition += BWAPI::Position(-1, 1);
						else if (vultureUnit->getID() % 4 == 3)
							mineSetPosition += BWAPI::Position(-1, -1);
						if (!mineSetPosition.isValid())
						{							
							mineSetPosition = vultureUnit->getPosition();
							break;
						}
					}
					Micro::SmartLaySpiderMine(vultureUnit, mineSetPosition);
					//BWAPI::UnitCommand currentCommand(vultureUnit->getLastCommand());
					//if (mineCount == vultureUnit->getSpiderMineCount() && currentCommand.getTargetPosition() == vultureUnit->getPosition() )
					//{
					//	if (vultureUnit->canUseTech(BWAPI::TechTypes::Spider_Mines, vultureUnit->getPosition() - BWAPI::Position(0, 1)))
					//	{
					//		//std::cout << " vulture No.  " << vultureUnit->getID() << " set Mine pos ( " << mineSetPosition.x << ", " << mineSetPosition.y << ")" << std::endl;
					//		vultureUnit->useTech(BWAPI::TechTypes::Spider_Mines, vultureUnit->getPosition() - BWAPI::Position(0, 1));
					//		//Micro::SmartLaySpiderMine(vultureUnit, mineSetPosition + BWAPI::Position(1, 0));
					//		//chokePointForVulture[(vultureUnit->getID() + vultureUnit->getSpiderMineCount()) % secondChokePointCount] -= BWAPI::Position(0, 1);
					//	}
					//}
					//std::cout << "getSpiderMineCount>0  69 done " << std::endl;
					continue;
				}
			}
			else 
			{
				BWAPI::Unit target = getTarget(vultureUnit, vultureUnitTargets);
				if (vultureUnit->getSpiderMineCount() > 0)
					Micro::SmartLaySpiderMine(vultureUnit, vultureUnit->getPosition());
			}


			// if there are targets
			if (!vultureUnitTargets.empty())
			{
				// find the best target for this zealot
				BWAPI::Unit target = getTarget(vultureUnit, vultureUnitTargets);

				// attack it
//					if (vultureUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || vultureUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
				{
					Micro::MutaDanceTarget(vultureUnit, target);
				}
				
			}
			// if there are no targets
			else
			{
				if (vultureUnit->getDistance(order.getPosition()) > 100 && order.getFarUnit()->getDistance(vultureUnit->getPosition()) > BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 100 && order.getType() == SquadOrderTypes::Attack && order.getFarUnit()->getID() != vultureUnit->getID())
				{
					//std::cout << "Marin  " << vultureUnit->getID() << std::endl;
					Micro::SmartMove(vultureUnit, order.getFarUnit()->getPosition() - vultureUnit->getPosition() + vultureUnit->getPosition());
				}
				else
					// if we're not near the order position
					if (vultureUnit->getDistance(order.getPosition()) > 100)
					{
						// move to it
						Micro::SmartAttackMove(vultureUnit, order.getPosition());
					}
			}
		}
	}
}

std::pair<BWAPI::Unit, BWAPI::Unit> VultureManager::findClosestUnitPair(const BWAPI::Unitset & attackers, const BWAPI::Unitset & targets)
{
	std::pair<BWAPI::Unit, BWAPI::Unit> closestPair(nullptr, nullptr);
	double closestDistance = std::numeric_limits<double>::max();

	for (auto & attacker : attackers)
	{
		BWAPI::Unit target = getTarget(attacker, targets);
		double dist = attacker->getDistance(attacker);

		if (!closestPair.first || (dist < closestDistance))
		{
			closestPair.first = attacker;
			closestPair.second = target;
			closestDistance = dist;
		}
	}

	return closestPair;
}

// get a target for the zealot to attack
BWAPI::Unit VultureManager::getTarget(BWAPI::Unit vultureUnit, const BWAPI::Unitset & targets)
{
	int bestPriorityDistance = 1000000;
	int bestPriority = 0;

	double bestLTD = 0;

	int highPriority = 0;
	double closestDist = std::numeric_limits<double>::infinity();
	BWAPI::Unit closestTarget = nullptr;

	for (const auto & target : targets)
	{
		double distance = vultureUnit->getDistance(target);
		double LTD = UnitUtil::CalculateLTD(target, vultureUnit);
		int priority = getAttackPriority(vultureUnit, target);
		bool targetIsThreat = LTD > 0;

		if (!closestTarget || (priority > highPriority) || (priority == highPriority && distance < closestDist))
		{
			closestDist = distance;
			highPriority = priority;
			closestTarget = target;
		}
	}

	return closestTarget;
}

// get the attack priority of a type in relation to a zergling
int VultureManager::getAttackPriority(BWAPI::Unit vultureUnit, BWAPI::Unit target)
{
	BWAPI::UnitType rangedType = vultureUnit->getType();
	BWAPI::UnitType targetType = target->getType();


	if (vultureUnit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
	{
		if (target->getType() == BWAPI::UnitTypes::Protoss_Carrier)
		{

			return 100;
		}

		if (target->getType() == BWAPI::UnitTypes::Protoss_Corsair)
		{
			return 90;
		}
	}

	bool isThreat = rangedType.isFlyer() ? targetType.airWeapon() != BWAPI::WeaponTypes::None : targetType.groundWeapon() != BWAPI::WeaponTypes::None;

	if (target->getType().isWorker())
	{
		isThreat = false;
	}

	if (target->getType() == BWAPI::UnitTypes::Zerg_Larva || target->getType() == BWAPI::UnitTypes::Zerg_Egg)
	{
		return 0;
	}

	if (vultureUnit->isFlying() && target->getType() == BWAPI::UnitTypes::Protoss_Carrier)
	{
		return 101;
	}

	// if the target is building something near our base something is fishy
	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()) && target->getDistance(ourBasePosition) < 1200)
	{
		return 100;
	}

	if (target->getType().isBuilding() && (target->isCompleted() || target->isBeingConstructed()) && target->getDistance(ourBasePosition) < 1200)
	{
		return 90;
	}

	// highest priority is something that can attack us or aid in combat
	if (targetType == BWAPI::UnitTypes::Terran_Bunker || isThreat)
	{
		return 11;
	}
	// next priority is worker
	else if (targetType.isWorker())
	{
		if (vultureUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
		{
			return 11;
		}

		return 11;
	}
	// next is special buildings
	else if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
	{
		return 5;
	}
	// next is special buildings
	else if (targetType == BWAPI::UnitTypes::Protoss_Pylon)
	{
		return 5;
	}
	// next is buildings that cost gas
	else if (targetType.gasPrice() > 0)
	{
		return 4;
	}
	else if (targetType.mineralPrice() > 0)
	{
		return 3;
	}
	// then everything else
	else
	{
		return 1;
	}
}

BWAPI::Unit VultureManager::closestvultureUnit(BWAPI::Unit target, std::set<BWAPI::Unit> & vultureUnitsToAssign)
{
	double minDistance = 0;
	BWAPI::Unit closest = nullptr;

	for (auto & vultureUnit : vultureUnitsToAssign)
	{
		double distance = vultureUnit->getDistance(target);
		if (!closest || distance < minDistance)
		{
			minDistance = distance;
			closest = vultureUnit;
		}
	}

	return closest;
}


// still has bug in it somewhere, use Old version
void VultureManager::assignTargetsNew(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & vultureUnits = getUnits();

	// figure out targets
	BWAPI::Unitset vultureUnitTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(vultureUnitTargets, vultureUnitTargets.end()), [](BWAPI::Unit u){ return u->isVisible(); });

	BWAPI::Unitset vultureUnitsToAssign(vultureUnits);
	std::map<BWAPI::Unit, int> attackersAssigned;

	for (auto & unit : vultureUnitTargets)
	{
		attackersAssigned[unit] = 0;
	}

	// keep assigning targets while we have attackers and targets remaining
	while (!vultureUnitsToAssign.empty() && !vultureUnitTargets.empty())
	{
		auto attackerAssignment = findClosestUnitPair(vultureUnitsToAssign, vultureUnitTargets);
		BWAPI::Unit & attacker = attackerAssignment.first;
		BWAPI::Unit & target = attackerAssignment.second;

		UAB_ASSERT_WARNING(attacker, "We should have chosen an attacker!");

		if (!attacker)
		{
			break;
		}

		if (!target)
		{
			Micro::SmartAttackMove(attacker, order.getPosition());
			continue;
		}

		if (Config::Micro::KiteWithRangedUnits)
		{
			if (attacker->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || attacker->getType() == BWAPI::UnitTypes::Terran_Vulture)
			{
				Micro::MutaDanceTarget(attacker, target);
			}
			else
			{
				Micro::SmartKiteTarget(attacker, target);
			}
		}
		else
		{
			Micro::SmartAttackUnit(attacker, target);
		}

		// update the number of units assigned to attack the target we found
		int & assigned = attackersAssigned[attackerAssignment.second];
		assigned++;

		// if it's a small / fast unit and there's more than 2 things attacking it already, don't assign more
		if ((target->getType().isWorker() || target->getType() == BWAPI::UnitTypes::Zerg_Zergling) && (assigned > 2))
		{
			vultureUnitTargets.erase(target);
		}
		// if it's a building and there's more than 10 things assigned to it already, don't assign more
		else if (target->getType().isBuilding() && (assigned > 10))
		{
			vultureUnitTargets.erase(target);
		}

		vultureUnitsToAssign.erase(attacker);
	}

	// if there's no targets left, attack move to the order destination
	if (vultureUnitTargets.empty())
	{
		for (auto & unit : vultureUnitsToAssign)
		{
			if (unit->getDistance(order.getPosition()) > 100)
			{
				// move to it
				Micro::SmartAttackMove(unit, order.getPosition());
			}
		}
	}
}