#include "CombatCommander.h"
#include "UnitUtil.h"
#include "MedicManager.h"

using namespace MyBot;

const size_t IdlePriority = 0;
const size_t AttackPriority = 1;
const size_t BaseDefensePriority = 2;
const size_t ScoutDefensePriority = 3;
const size_t DropPriority = 4;

CombatCommander & CombatCommander::Instance()
{
	static CombatCommander instance;
	return instance;
}

CombatCommander::CombatCommander() 
    : _initialized(false)
{

}

void CombatCommander::initializeSquads()
{
	SquadOrder idleOrder(SquadOrderTypes::Attack, InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter(), 100, "Chill Out");
	_squadData.addSquad("Idle", Squad("Idle", idleOrder, IdlePriority));

    // the main attack squad that will pressure the enemy's closest base location
    SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
	_squadData.addSquad("MainAttack", Squad("MainAttack", mainAttackOrder, AttackPriority));

    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

    // the scout defense squad will handle chasing the enemy worker scout
	SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, ourBasePosition, ourBasePosition.getDistance(InformationManager::Instance().getFirstChokePoint(BWAPI::Broodwar->self())->getCenter()), "Kill the scout");
    _squadData.addSquad("ScoutDefense", Squad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority));
	
	////BaseDefensePriority
	//SquadOrder baseDefense(SquadOrderTypes::Defend, ourBasePosition, ourBasePosition.getDistance(InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter()), "Handle In Base Enemy");
	//_squadData.addSquad("BaseDefense", Squad("BaseDefense", baseDefense, BaseDefensePriority));

    // add a drop squad if we are using a drop strategy
    if (Config::Strategy::StrategyName == "Protoss_Drop")
    {
		SquadOrder zealotDrop(SquadOrderTypes::Drop, ourBasePosition, 900, "Wait for transport");
        _squadData.addSquad("Drop", Squad("Drop", zealotDrop, DropPriority));
    }

    _initialized = true;
}

bool CombatCommander::isSquadUpdateFrame()
{
	return BWAPI::Broodwar->getFrameCount() % 10 == 0;
}

void CombatCommander::update(const BWAPI::Unitset & combatUnits)
{
    if (!Config::Modules::UsingCombatCommander)
    {
        return;
    }

    if (!_initialized)
    {
        initializeSquads();
    }

    _combatUnits = combatUnits;


	if (isSquadUpdateFrame())
	{		
        updateIdleSquad();		
        updateDropSquads();
        updateScoutDefenseSquad();		
		updateDefenseSquads();		
		updateAttackSquads();
	}
	
	_squadData.update();
	
}

void CombatCommander::updateIdleSquad()
{
	int radi = 300;
    Squad & idleSquad = _squadData.getSquad("Idle");	
	//if (_combatUnits.size() % 10 == 1)
	{
		int diff_x = InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter().x - InformationManager::Instance().getFirstExpansionLocation(BWAPI::Broodwar->self())->getPosition().x;
		int diff_y = InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter().y - InformationManager::Instance().getFirstExpansionLocation(BWAPI::Broodwar->self())->getPosition().y;
		if (abs(diff_x) > abs(diff_y))
		{
			SquadOrder idleOrder(SquadOrderTypes::Attack, InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter()
				+ BWAPI::Position(_combatUnits.size(), 32), radi, "Move Out");
			idleSquad.setSquadOrder(idleOrder);
		}
		else{
			SquadOrder idleOrder(SquadOrderTypes::Attack, InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter()
				+ BWAPI::Position(32, _combatUnits.size()), radi, "Move Out");
			idleSquad.setSquadOrder(idleOrder);
		}

		BWAPI::Position mineralPosition = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getPosition();
		for (auto & unit : _combatUnits)
		{
			// if it hasn't been assigned to a squad yet, put it in the low priority idle squad
			//BWAPI::UnitCommand currentCommand(unit->getLastCommand());
			if (_squadData.canAssignUnitToSquad(unit, idleSquad))
			{
				//idleSquad.addUnit(unit);
				_squadData.assignUnitToSquad(unit, idleSquad);
				if (mineralPosition == InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getPosition())
					for (auto &unit_in_region : unit->getUnitsInRadius(600)){
						if (unit_in_region->getType() == BWAPI::UnitTypes::Resource_Mineral_Field){
							mineralPosition = unit_in_region->getPosition();
							break;
						}
					}
			}
		}


		if (idleSquad.getUnits().size() < 13)
		{
			int tmp_radi = 190;
			SquadOrder idleOrder(SquadOrderTypes::Attack, mineralPosition
				, tmp_radi, "Move Out");
			idleSquad.setSquadOrder(idleOrder);
		}
		//else if (idleSquad.getUnits().size() < 15)
		//{
		//	SquadOrder idleOrder(SquadOrderTypes::Attack, InformationManager::Instance().getFirstExpansionLocation(BWAPI::Broodwar->self())->getPosition()
		//		, 100, "Move Out");
		//	idleSquad.setSquadOrder(idleOrder);
		//}
		else if (idleSquad.getUnits().size() < 17)
		{
			SquadOrder idleOrder(SquadOrderTypes::Attack, InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter()
				, radi, "Move Out");
			idleSquad.setSquadOrder(idleOrder);
		}
		else{
			SquadOrder idleOrder(SquadOrderTypes::Attack,
				InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter() - InformationManager::Instance().getFirstExpansionLocation(BWAPI::Broodwar->self())->getPosition()
				+ InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter()
				, radi, "Move Out");
			idleSquad.setSquadOrder(idleOrder);
		}

	}
    
}

void CombatCommander::updateAttackSquads()
{
	Squad & mainAttackSquad = _squadData.getSquad("MainAttack");
	Squad & candiAttackerSquad = _squadData.getSquad("Idle");

	if ((mainAttackSquad.getUnits().size() > 7) && candiAttackerSquad.getUnits().size() > 10)
	{
		for (auto & unit : _combatUnits)
		{
			if (_squadData.canAssignUnitToSquad(unit, mainAttackSquad))
			{
				_squadData.assignUnitToSquad(unit, mainAttackSquad);
			}
		}
	}
	else if (candiAttackerSquad.getUnits().size() > 20)
	{
		for (auto & unit : _combatUnits)
		{
			if (_squadData.canAssignUnitToSquad(unit, mainAttackSquad))
			{
				_squadData.assignUnitToSquad(unit, mainAttackSquad);
			}
		}
	}

	SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocationForCombat(mainAttackSquad.calcCenter()) , 800, "Attack Enemy ChokePoint");
	mainAttackSquad.setSquadOrder(mainAttackOrder);
}

void CombatCommander::updateDropSquads()
{
    if (Config::Strategy::StrategyName != "Protoss_Drop")
    {
        return;
    }

    Squad & dropSquad = _squadData.getSquad("Drop");

    // figure out how many units the drop squad needs
    bool dropSquadHasTransport = false;
    int transportSpotsRemaining = 8;
    auto & dropUnits = dropSquad.getUnits();

    for (auto & unit : dropUnits)
    {
        if (unit->isFlying() && unit->getType().spaceProvided() > 0)
        {
            dropSquadHasTransport = true;
        }
        else
        {
            transportSpotsRemaining -= unit->getType().spaceRequired();
        }
    }

    // if there are still units to be added to the drop squad, do it
    if (transportSpotsRemaining > 0 || !dropSquadHasTransport)
    {
        // take our first amount of combat units that fill up a transport and add them to the drop squad
        for (auto & unit : _combatUnits)
        {
            // if this is a transport unit and we don't have one in the squad yet, add it
            if (!dropSquadHasTransport && (unit->getType().spaceProvided() > 0 && unit->isFlying()))
            {
                _squadData.assignUnitToSquad(unit, dropSquad);
                dropSquadHasTransport = true;
                continue;
            }

            if (unit->getType().spaceRequired() > transportSpotsRemaining)
            {
                continue;
            }

            // get every unit of a lower priority and put it into the attack squad
            if (!unit->getType().isWorker() && _squadData.canAssignUnitToSquad(unit, dropSquad))
            {
                _squadData.assignUnitToSquad(unit, dropSquad);
                transportSpotsRemaining -= unit->getType().spaceRequired();
            }
        }
    }
    // otherwise the drop squad is full, so execute the order
    else
    {
        SquadOrder dropOrder(SquadOrderTypes::Drop, getMainAttackLocation(), 800, "Attack Enemy Base");
        dropSquad.setSquadOrder(dropOrder);
    }
}

void CombatCommander::updateScoutDefenseSquad() 
{
    if (_combatUnits.empty()) 
    { 
        return; 
    }

    // if the current squad has units in it then we can ignore this
    Squad & scoutDefenseSquad = _squadData.getSquad("ScoutDefense");
  
    // get the region that our base is located in
    BWTA::Region * myRegion = BWTA::getRegion(BWAPI::Broodwar->self()->getStartLocation());
    if (!myRegion && myRegion->getCenter().isValid())
    {
        return;
    }

    // get all of the enemy units in this region
	BWAPI::Unitset enemyUnitsInRegion;
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
        {
            enemyUnitsInRegion.insert(unit);
        }
    }

    // if there's an enemy worker in our region then assign someone to chase him
    bool assignScoutDefender = enemyUnitsInRegion.size() == 1 && (*enemyUnitsInRegion.begin())->getType().isWorker();

    // if our current squad is empty and we should assign a worker, do it
    if (scoutDefenseSquad.isEmpty() && assignScoutDefender)
    {
        // the enemy worker that is attacking us
        BWAPI::Unit enemyWorker = *enemyUnitsInRegion.begin();

        // get our worker unit that is mining that is closest to it
        BWAPI::Unit workerDefender = findClosestWorkerToTarget(_combatUnits, enemyWorker);

		if (enemyWorker && workerDefender)
		{
			// grab it from the worker manager and put it in the squad
            if (_squadData.canAssignUnitToSquad(workerDefender, scoutDefenseSquad))
            {
			    WorkerManager::Instance().setCombatWorker(workerDefender);
                _squadData.assignUnitToSquad(workerDefender, scoutDefenseSquad);
            }
		}
    }
    // if our squad is not empty and we shouldn't have a worker chasing then take him out of the squad
    else if (!scoutDefenseSquad.isEmpty() && !assignScoutDefender)
    {
        for (auto & unit : scoutDefenseSquad.getUnits())
        {
            unit->stop();
            if (unit->getType().isWorker())
            {
                WorkerManager::Instance().finishedWithWorker(unit);
            }
        }

        scoutDefenseSquad.clear();
    }
}

void CombatCommander::updateDefenseSquads() 
{
	if (_combatUnits.empty() || _combatUnits.size() == 0)
    { 
        return; 
    }
    
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
    BWTA::Region * enemyRegion = nullptr;
    if (enemyBaseLocation)
    {
        enemyRegion = BWTA::getRegion(enemyBaseLocation->getPosition());
    }

	// for each of our occupied regions
	for (BWTA::Region * myRegion : InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->self()))
	{
        // don't defend inside the enemy region, this will end badly when we are stealing gas
        if (myRegion == enemyRegion)
        {
            continue;
        }

		BWAPI::Position regionCenter = myRegion->getCenter();
		if (!regionCenter.isValid())
		{
			continue;
		}

		// start off assuming all enemy units in region are just workers
		int numDefendersPerEnemyUnit = 2;

		// all of the enemy units in this region
		BWAPI::Unitset enemyUnitsInRegion;
        for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            // if it's an overlord, don't worry about it for defense, we don't care what they see
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
            {
                continue;
            }

            if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
            {
				//std::cout << "enemyUnits In My Region " << std::endl;
                enemyUnitsInRegion.insert(unit);
            }
        }

        // we can ignore the first enemy worker in our region since we assume it is a scout
        for (auto & unit : enemyUnitsInRegion)
        {
            if (unit->getType().isWorker())
            {
                enemyUnitsInRegion.erase(unit);
                break;
            }
        }

        int numEnemyFlyingInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return u->isFlying(); });
        int numEnemyGroundInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return !u->isFlying(); });

        std::stringstream squadName;
		squadName << "Base Defense " << regionCenter.x << " " << regionCenter.y;
        
        // if there's nothing in this region to worry about
        if (enemyUnitsInRegion.empty())
        {
            // if a defense squad for this region exists, remove it
            if (_squadData.squadExists(squadName.str()))
            {
                _squadData.getSquad(squadName.str()).clear();
            }
            
            // and return, nothing to defend here
            continue;
        }
        else 
        {
            // if we don't have a squad assigned to this region already, create one
            if (!_squadData.squadExists(squadName.str()))
            {
				std::cout << "Defend My Region!" << std::endl;
                SquadOrder defendRegion(SquadOrderTypes::Defend, regionCenter, 32 * 25, "Defend Region!");
                _squadData.addSquad(squadName.str(), Squad(squadName.str(), defendRegion, BaseDefensePriority));
            }
        }

        // assign units to the squad
        if (_squadData.squadExists(squadName.str()))
        {
            Squad & defenseSquad = _squadData.getSquad(squadName.str());

            // figure out how many units we need on defense
	        int flyingDefendersNeeded = numDefendersPerEnemyUnit * numEnemyFlyingInRegion;
	        int groundDefensersNeeded = numDefendersPerEnemyUnit * numEnemyGroundInRegion;
			
            updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefensersNeeded);
        }
        else
        {
            UAB_ASSERT_WARNING(false, "Squad should have existed: %s", squadName.str().c_str());
        }
	}

    // for each of our defense squads, if there aren't any enemy units near the position, remove the squad
    std::set<std::string> uselessDefenseSquads;
    for (const auto & kv : _squadData.getSquads())
    {
        const Squad & squad = kv.second;
        const SquadOrder & order = squad.getSquadOrder();

        if (order.getType() != SquadOrderTypes::Defend)
        {
            continue;
        }

        bool enemyUnitInRange = false;
        for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (unit->getPosition().getDistance(order.getPosition()) < order.getRadius())
            {
                enemyUnitInRange = true;
                break;
            }
        }

        if (!enemyUnitInRange)
        {
            _squadData.getSquad(squad.getName()).clear();
        }
    }
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded)
{
	//@도주남 김지훈 기존로직에서 변경함
	//const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();
	const BWAPI::Unitset & squadUnits = _squadData.getSquad("Idle").getUnits();
    size_t flyingDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackAir);
    size_t groundDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackGround);
	//std::cout << "flyingDefendersInSquad " << flyingDefendersInSquad << " groundDefendersInSquad " << groundDefendersInSquad << std::endl;
    // if there's nothing left to defend, clear the squad
    if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
    {
        defenseSquad.clear();
        return;
    }

    // add flying defenders if we still need them
    size_t flyingDefendersAdded = 0;
    while (flyingDefendersNeeded > flyingDefendersInSquad + flyingDefendersAdded)
    {
        BWAPI::Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), true);

        // if we find a valid flying defender, add it to the squad
        if (defenderToAdd)
        {
            _squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
            ++flyingDefendersAdded;
        }
        // otherwise we'll never find another one so break out of this loop
        else
        {
            break;
        }
    }

    // add ground defenders if we still need them
    size_t groundDefendersAdded = 0;
    while (groundDefendersNeeded > groundDefendersInSquad + groundDefendersAdded)
    {
        BWAPI::Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), false);

        // if we find a valid ground defender add it
        if (defenderToAdd)
        {
            _squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
            ++groundDefendersAdded;
        }
        // otherwise we'll never find another one so break out of this loop
        else
        {
            break;
        }
    }
}

BWAPI::Unit CombatCommander::findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender) 
{
	BWAPI::Unit closestDefender = nullptr;
	double minDistance = std::numeric_limits<double>::max();

    int zerglingsInOurBase = numZerglingsInOurBase();
    bool zerglingRush = zerglingsInOurBase > 0 && BWAPI::Broodwar->getFrameCount() < 5000;

	for (auto & unit : _combatUnits) 
	{
		if ((flyingDefender && !UnitUtil::CanAttackAir(unit)) || (!flyingDefender && !UnitUtil::CanAttackGround(unit)))
        {
            continue;
        }

        if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
        {
            continue;
        }

        // add workers to the defense squad if we are being rushed very quickly
        if (!Config::Micro::WorkersDefendRush || (unit->getType().isWorker() && !zerglingRush && !beingBuildingRushed()))
        {
            continue;
        }

        double dist = unit->getDistance(pos);
        if (!closestDefender || (dist < minDistance))
        {
            closestDefender = unit;
            minDistance = dist;
        }
	}

	return closestDefender;
}

BWAPI::Position CombatCommander::getDefendLocation()
{
	return BWTA::getRegion(BWTA::getStartLocation(BWAPI::Broodwar->self())->getTilePosition())->getCenter();
}

void CombatCommander::drawSquadInformation(int x, int y)
{
	_squadData.drawSquadInformation(x, y);
}

BWAPI::Position CombatCommander::getMainAttackLocationForCombat(BWAPI::Position ourCenterPosition)
{
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
	BWTA::Chokepoint * enemySecondCP = InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->enemy());
	
	if (enemySecondCP)
	{
		BWAPI::Position enemySecondChokePosition = enemySecondCP->getCenter();
		//std::cout << "MapTools::Instance().getGroundDistance(enemySecondChokePosition, enemyBaseLocation->getPosition()) " << MapTools::Instance().getGroundDistance(enemySecondChokePosition, enemyBaseLocation->getPosition()) << std::endl;
		//std::cout << "MapTools::Instance().getGroundDistance(ourCenterPosition, enemyBaseLocation->getPosition())) " << MapTools::Instance().getGroundDistance(ourCenterPosition, enemyBaseLocation->getPosition()) << std::endl;
		//std::cout << "enemySecondCP->getWidth() " << enemySecondCP->getWidth() << std::endl;
		//std::cout << "MapTools::Instance().getGroundDistance(enemySecondChokePosition, ourCenterPosition) " << MapTools::Instance().getGroundDistance(enemySecondChokePosition, ourCenterPosition) << std::endl << std::endl;

		if (MapTools::Instance().getGroundDistance(enemySecondChokePosition, ourCenterPosition) > 5 //enemySecondCP->getWidth()
			&& MapTools::Instance().getGroundDistance(enemySecondChokePosition, enemyBaseLocation->getPosition())
			< MapTools::Instance().getGroundDistance(ourCenterPosition, enemyBaseLocation->getPosition()))
			return enemySecondChokePosition;
	}
	return getMainAttackLocation();

}

BWAPI::Position CombatCommander::getMainAttackLocation()
{
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
	//BWTA::Chokepoint * enemySecondCP = InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->enemy());
	//
	//if (enemySecondCP)
	//{
	//	BWAPI::Position enemySecondChokePosition = enemySecondCP->getCenter();
	//	
	//}

	// First choice: Attack an enemy region if we can see units inside it
    if (enemyBaseLocation)
    {
        BWAPI::Position enemyBasePosition = enemyBaseLocation->getPosition();

        // get all known enemy units in the area
        BWAPI::Unitset enemyUnitsInArea;
		MapGrid::Instance().getUnitsNear(enemyUnitsInArea, enemyBasePosition, 800, false, true);

        bool onlyOverlords = true;
        for (auto & unit : enemyUnitsInArea)
        {
            if (unit->getType() != BWAPI::UnitTypes::Zerg_Overlord)
            {
                onlyOverlords = false;
            }
        }

        if (!BWAPI::Broodwar->isExplored(BWAPI::TilePosition(enemyBasePosition)) || !enemyUnitsInArea.empty())
        {
            if (!onlyOverlords)
            {
                return enemyBaseLocation->getPosition();
            }
        }
    }

    // Second choice: Attack known enemy buildings
    for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
    {
        const UnitInfo & ui = kv.second;

        if (ui.type.isBuilding() && ui.lastPosition != BWAPI::Positions::None)
		{
			return ui.lastPosition;	
		}
    }

    // Third choice: Attack visible enemy units that aren't overlords
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
        if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
        {
            continue;
        }

		if (UnitUtil::IsValidUnit(unit) && unit->isVisible())
		{
			return unit->getPosition();
		}
	}

    // Fourth choice: We can't see anything so explore the map attacking along the way
    return MapGrid::Instance().getLeastExplored();
}

BWAPI::Unit CombatCommander::findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target)
{
    UAB_ASSERT(target != nullptr, "target was null");

    if (!target)
    {
        return nullptr;
    }

    BWAPI::Unit closestMineralWorker = nullptr;
    double closestDist = 100000;
    
    // for each of our workers
	for (auto & unit : unitsToAssign)
	{
        if (!unit->getType().isWorker())
        {
            continue;
        }

		// if it is a move worker
        if (WorkerManager::Instance().isMineralWorker(unit)) 
		{
			double dist = unit->getDistance(target);

            if (!closestMineralWorker || dist < closestDist)
            {
                closestMineralWorker = unit;
                dist = closestDist;
            }
		}
	}

    return closestMineralWorker;
}

// when do we want to defend with our workers?
// this function can only be called if we have no fighters to defend with
int CombatCommander::defendWithWorkers()
{
	// our home nexus position
	BWAPI::Position homePosition = BWTA::getStartLocation(BWAPI::Broodwar->self())->getPosition();;

	// enemy units near our workers
	int enemyUnitsNearWorkers = 0;

	// defense radius of nexus
	int defenseRadius = 300;

	// fill the set with the types of units we're concerned about
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		// if it's a zergling or a worker we want to defend
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
		{
			if (unit->getDistance(homePosition) < defenseRadius)
			{
				enemyUnitsNearWorkers++;
			}
		}
	}

	// if there are enemy units near our workers, we want to defend
	return enemyUnitsNearWorkers;
}

int CombatCommander::numZerglingsInOurBase()
{
    int concernRadius = 600;
    int zerglings = 0;
    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    
    // check to see if the enemy has zerglings as the only attackers in our base
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType() != BWAPI::UnitTypes::Zerg_Zergling)
        {
            continue;
        }

        if (unit->getDistance(ourBasePosition) < concernRadius)
        {
            zerglings++;
        }
    }

    return zerglings;
}

bool CombatCommander::beingBuildingRushed()
{
    int concernRadius = 1200;
    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    
    // check to see if the enemy has zerglings as the only attackers in our base
    for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->getType().isBuilding())
        {
            return true;
        }
    }

    return false;
}