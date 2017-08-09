#include "VultureManager.h"
#include "CommandUtil.h"

using namespace MyBot;

VultureManager::VultureManager()
{
	miningOn = false;
	miningUnit = nullptr;
}

void VultureManager::miningPositionSetting()
{
	BWTA::Chokepoint * mysecChokePoint = InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self());
	BWTA::Chokepoint * enemySecChokePoint = InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->enemy());
	
	if (enemySecChokePoint == nullptr || getUnits().size() == 0 || miningOn)
		return;

	if (!BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Spider_Mines))
		return;

	startPointCount = 0;

	std::list<BWTA::BaseLocation *> enemayBaseLocations = InformationManager::Instance().getOccupiedBaseLocations(BWAPI::Broodwar->enemy());
	std::list<BWTA::BaseLocation *> selfBaseLocations = InformationManager::Instance().getOccupiedBaseLocations(BWAPI::Broodwar->self());
	
	for (BWTA::BaseLocation * startLocation : BWTA::getBaseLocations())
	{
		bool insertable = true;
		if (InformationManager::Instance().getFirstExpansionLocation(BWAPI::Broodwar->enemy()) == startLocation)
			continue;
		for (BWTA::BaseLocation * eBaseLocation : enemayBaseLocations)
		{
			if (startLocation == eBaseLocation)
			{
				insertable = false;
				break;
			}
		}
		if (insertable)
		{
			for (BWTA::BaseLocation * sBaseLocation : selfBaseLocations)
			{
				if (startLocation == sBaseLocation)
				{
					insertable = false;
					break;
				}
			}
		}
		if (insertable)
		{
			if (BWTA::isConnected(startLocation->getTilePosition(), InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getTilePosition()) == false)
			{
				insertable = false;
			}
		}
		if (insertable)
		{
			chokePointForVulture.push_back(startLocation->getPosition());
		}
	}
	startPointCount = chokePointForVulture.size();

	std::vector<BWAPI::TilePosition> tileList = BWTA::getShortestPath(BWAPI::TilePosition(mysecChokePoint->getCenter()), BWAPI::TilePosition(enemySecChokePoint->getCenter()));
	
	for (auto & t : tileList) {
		BWAPI::Position tp(t.x*32, t.y*32);
		if (!tp.isValid())
			continue;
		if (tp.getDistance(enemySecChokePoint->getCenter()) < 120)
			continue;
		if (tp.getDistance(mysecChokePoint->getCenter()) < 100)
			continue;		
		chokePointForVulture.push_back(tp);
	}
	pathTileCount = chokePointForVulture.size();

	for (BWTA::Chokepoint * chokepoint : BWTA::getChokepoints())
	{
		if (chokepoint == InformationManager::Instance().getFirstChokePoint(BWAPI::Broodwar->self()))
			continue;
		if (chokepoint == mysecChokePoint)
			continue;
		if (chokepoint == InformationManager::Instance().getFirstChokePoint(BWAPI::Broodwar->enemy()))
			continue;
		if (chokepoint == enemySecChokePoint)
			continue;
		chokePointForVulture.push_back(chokepoint->getCenter());	
	}
	miningOn = true;
	return;

}

void VultureManager::executeMicro(const BWAPI::Unitset & targets)
{
	assignTargetsOld(targets);
}


void VultureManager::assignTargetsOld(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & vultureUnits = getUnits();
	int spiderMineCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Vulture_Spider_Mine);
	// figure out targets
	BWAPI::Unitset vultureUnitTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(vultureUnitTargets, vultureUnitTargets.end()), [](BWAPI::Unit u){ return u->isVisible(); });	
	
	if (miningUnit!=nullptr)
	{
		if ((miningUnit->getSpiderMineCount() == 0 || miningUnit->getHitPoints() <= 0 || miningUnit->isStuck() || !vultureUnits.contains(miningUnit)) && BWAPI::Broodwar->getFrameCount() % 500 ==0)
		{
			miningUnit = nullptr;
		} 
		else
			BWAPI::Broodwar->drawCircleMap(miningUnit->getPosition(), 10, BWAPI::Colors::Red, false);
	}
	for (auto & vultureUnit : vultureUnits)
	{
		// train sub units such as scarabs or interceptors
		//trainSubUnits(vultureUnit);
		//if (order.getType() == SquadOrderTypes::MineInstallation)
		// 만들까 말까 고민됨 
		if (order.getType() == SquadOrderTypes::Drop)
		{
			if (BWTA::getRegion(BWAPI::TilePosition(vultureUnit->getPosition())) != InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion())
			{
				if (vultureUnit->getSpiderMineCount() == 3)
					Micro::SmartLaySpiderMine(vultureUnit, vultureUnit->getPosition());
				else
				{
					if (!vultureUnitTargets.empty())
					{
						// find the best target for this zealot
						BWAPI::Unit target = getTarget(vultureUnit, vultureUnitTargets);
						vultureUnit->attack(target);
						//Micro::MutaDanceTarget(vultureUnit, target);
					}
				}
			}
			continue;
		}
		//else
		// if the order is to attack or defend
		if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend || order.getType() == SquadOrderTypes::Idle)
		{
			
			if (order.getType() == SquadOrderTypes::Idle)
			{
				if (vultureUnit->getSpiderMineCount() > 0 && chokePointForVulture.size() > 0 && (miningUnit == nullptr || miningUnit == vultureUnit) && !vultureUnit->isStuck())
				{
					//std::cout << "spiderMineCount " << spiderMineCount << std::endl;
					int minV = 999999;
					int index = -1;
					BWAPI::Position mineSetPosition = vultureUnit->getPosition();
					
					//@도주남 김지훈 스파이더마인 설치를 지나가는 패스에 우선적으로 설치한다.
					if (spiderMineCount < pathTileCount)
						mineSetPosition = chokePointForVulture[spiderMineCount];
					else
						mineSetPosition 
						= chokePointForVulture[
							((vultureUnit->getID() + vultureUnit->getSpiderMineCount()) 
								% (chokePointForVulture.size() - pathTileCount)) + pathTileCount];
				
					while (!vultureUnit->canUseTechPosition(BWAPI::TechTypes::Spider_Mines, mineSetPosition) || BWAPI::Broodwar->getUnitsOnTile(BWAPI::TilePosition(mineSetPosition)).size() > 1)
					{						
						if (vultureUnit->getID() % 4 == 0)
							mineSetPosition += BWAPI::Position(1, 1);
						else if (vultureUnit->getID() % 4 == 1)
							mineSetPosition += BWAPI::Position(0, -1);
						else if (vultureUnit->getID() % 4 == 2)
							mineSetPosition += BWAPI::Position(-2, 0);
						else if (vultureUnit->getID() % 4 == 3)
							mineSetPosition += BWAPI::Position(-1, -1);
						if (!mineSetPosition.isValid())
						{							
							mineSetPosition = vultureUnit->getPosition();
							//break;
						}
					}
					BWAPI::Broodwar->drawTextMap(mineSetPosition, "%s", "Mine Set Here");
					Micro::SmartLaySpiderMine(vultureUnit, mineSetPosition);
					
					miningUnit = vultureUnit;
					continue;
				}
			}
			
			// if there are targets
			if (!vultureUnitTargets.empty())
			{
				// find the best target for this zealot
				BWAPI::Unit target = getTarget(vultureUnit, vultureUnitTargets);

				if (target->getDistance(order.getPosition()) / 32 > order.getRadius() - BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.groundWeapon().maxRange())
				{					
					vultureUnit->move(order.getPosition());
				}
				else
				{
					Micro::MutaDanceTarget(vultureUnit, target);
				}
				
			}
			// if there are no targets
			else
			{
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
