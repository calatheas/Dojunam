#include "CombatCommander.h"
#include "MedicManager.h"

using namespace MyBot;

const size_t IdlePriority = 0;
const size_t AttackPriority = 1;
const size_t BaseDefensePriority = 2;
//const size_t ScoutDefensePriority = 3;
const size_t DropPriority = 4;

CombatCommander & CombatCommander::Instance()
{
	static CombatCommander instance;
	return instance;
}

CombatCommander::CombatCommander() 
    : _initialized(false)
{

	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	//tstruct = *localtime(&now);
	localtime_s(&tstruct, &now);
	strftime(buf, sizeof(buf), "%Y%m%d%H%M%S_combat", &tstruct);

	log_file_path = Config::Strategy::WriteDir + std::string(buf) + ".log";
	std::cout << "log_file_path:" << log_file_path << std::endl;
	/////////////////////////////////////////////////////////////////////////
	
}

void CombatCommander::initializeSquads()
{
	initMainAttackPath = false;
	curIndex = 0;

	indexFirstChokePoint_OrderPosition = -1;
	rDefence_OrderPosition = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getPosition();
	BWAPI::Position myFirstChokePointCenter = InformationManager::Instance().getFirstChokePoint(BWAPI::Broodwar->self())->getCenter();
	BWAPI::Unit targetDepot = nullptr;
	double maxDistance = 0;
	for (auto & munit : BWAPI::Broodwar->getAllUnits())
	{
		if (
			(munit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)
			&& myFirstChokePointCenter.getDistance(munit->getPosition()) >= maxDistance
			&& BWTA::getRegion(BWAPI::TilePosition(munit->getPosition())) 
			== InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion()
			)
		{
			maxDistance = myFirstChokePointCenter.getDistance(munit->getPosition());
			targetDepot = munit;
		}
	}
	float dx = 0, dy = 0;
	if (targetDepot == nullptr)
		return;
	std::cout << "targetDepot " << targetDepot->getPosition().x/ 32 << " " << targetDepot->getPosition().y/32 << std::endl;
	dx = (targetDepot->getPosition().x - rDefence_OrderPosition.x)*0.7f;
	dy = (targetDepot->getPosition().y - rDefence_OrderPosition.y)*0.7f;
	rDefence_OrderPosition = rDefence_OrderPosition + BWAPI::Position(dx, dy);// (mineralPosition + closestDepot->getPosition()) / 2;
	wFirstChokePoint_OrderPosition = getPositionForDefenceChokePoint(InformationManager::Instance().getFirstChokePoint(BWAPI::Broodwar->self()));


	SquadOrder idleOrder(SquadOrderTypes::Attack, rDefence_OrderPosition, 100, "Chill Out");
	_squadData.addSquad("Idle", Squad("Idle", idleOrder, IdlePriority));

    // the main attack squad that will pressure the enemy's closest base location
    SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
	_squadData.addSquad("MainAttack", Squad("MainAttack", mainAttackOrder, AttackPriority));

    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

	SquadOrder zealotDrop(SquadOrderTypes::Drop, ourBasePosition, 900, "Wait for transport");
    _squadData.addSquad("Drop", Squad("Drop", zealotDrop, DropPriority));    

    _initialized = true;
}

bool CombatCommander::isSquadUpdateFrame()
{
	updateComBatStatusIndex();
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
		updateDefenseSquads();		
		updateAttackSquads();
		InformationManager::Instance().nowCombatStatus = _combatStatus;
	}
	
	_squadData.update();
	log_write("update END, ");
	drawSquadInformation(20, 200);
}

void CombatCommander::updateIdleSquad()
{
	int radi = 300;
	if (InformationManager::Instance().getMapName() == 'H')
	{
		radi = 500;
	}
    Squad & idleSquad = _squadData.getSquad("Idle");
		
	//BWAPI::Position mineralPosition = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getPosition();
	for (auto & unit : _combatUnits)
	{
		
		if (_squadData.canAssignUnitToSquad(unit, idleSquad))
		{
			//idleSquad.addUnit(unit);
			_squadData.assignUnitToSquad(unit, idleSquad);
		}
		


		if (_combatStatus <= InformationManager::combatStatus::rDefence)
		{
			int tmp_radi = 200;
			SquadOrder idleOrder(SquadOrderTypes::Idle, rDefence_OrderPosition
				, rDefence_OrderPosition.getDistance(wFirstChokePoint_OrderPosition), "Move Out");
			idleSquad.setSquadOrder(idleOrder);
		}
		else if (_combatStatus == InformationManager::combatStatus::wFirstChokePoint)
		{
			int addSquadRadi = 60;
			if (InformationManager::Instance().getMapName() == 'H')
			{
				addSquadRadi += 100;
			}
			SquadOrder idleOrder(SquadOrderTypes::Idle, wFirstChokePoint_OrderPosition
				, BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + addSquadRadi, "Move Out");
			idleSquad.setSquadOrder(idleOrder);
		}
		else if (_combatStatus == InformationManager::combatStatus::wSecondChokePoint)
		{
			SquadOrder idleOrder(SquadOrderTypes::Idle, getFirstChokePoint_OrderPosition()
				, radi, "Move Out");
			idleSquad.setSquadOrder(idleOrder);
		}
		else if (_combatStatus == InformationManager::combatStatus::rMainAttack){
			SquadOrder idleOrder(SquadOrderTypes::Idle,
				getIdleSquadLastOrderLocation()
				, radi, "Move Out");
			idleSquad.setSquadOrder(idleOrder);
		}
		log_write("Idlesquad END, ");
	}
    
}

BWAPI::Position CombatCommander::getIdleSquadLastOrderLocation()
{
	log_write("getIdleSquadLastOrderLocation , ");
	BWAPI::Position mCenter((BWAPI::Broodwar->mapWidth()*16), BWAPI::Broodwar->mapHeight()*16);
	BWTA::Chokepoint * mSec = InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self());
	int fIndex = 7;
	if (mainAttackPath.size() > 0)
	{
		if ((_combatUnits.size() / fIndex) + 1 < mainAttackPath.size())
			return mainAttackPath[(_combatUnits.size() / fIndex)];
	}

	return mSec->getCenter();

}

void CombatCommander::updateAttackSquads()
{
	Squad & mainAttackSquad = _squadData.getSquad("MainAttack");
	Squad & candiAttackerSquad = _squadData.getSquad("Idle");
	
	if (_combatStatus == InformationManager::combatStatus::gEnemybase)
	{
		for (auto & unit : _combatUnits)
		{
			//if (_dropUnits.contains(unit))
			//	continue;
			if (_squadData.canAssignUnitToSquad(unit, mainAttackSquad))
			{
				_squadData.assignUnitToSquad(unit, mainAttackSquad);
			}
		}
	}
	else if (_combatStatus == InformationManager::combatStatus::jMainAttack)
	{
		for (auto & unit : _combatUnits)
		{
			//if (_dropUnits.contains(unit))
			//	continue;
			if (_squadData.canAssignUnitToSquad(unit, mainAttackSquad))
			{
				_squadData.assignUnitToSquad(unit, mainAttackSquad);
			}
		}
	}

	SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocationForCombat(mainAttackSquad.calcCenter()), 410, "Attack Enemy ChokePoint");
	mainAttackSquad.setSquadOrder(mainAttackOrder);
}

void CombatCommander::updateDropSquads()
{
	Squad & dropSquad = _squadData.getSquad("Drop");
	
    // figure out how many units the drop squad needs
    bool dropSquadHasTransport = false;
    int transportSpotsRemaining = 8;
    auto & dropUnits = dropSquad.getUnits();
	BWAPI::Unit dropShipUnit = nullptr;
	for (auto & unit : dropUnits)
	{
		if (unit->isFlying() && unit->getType().spaceProvided() > 0)
		{
			dropShipUnit = unit;
		}
	}

    for (auto & unit : dropUnits)
    {
        if (unit->isFlying() && unit->getType().spaceProvided() > 0)
        {
            dropSquadHasTransport = true;
        }
        else
        {
			if (dropShipUnit)
			{
				if (!unit->isLoaded() 
					&& BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) 
					== InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion()
					)
				{
					dropShipUnit->load(unit);
				}
			}
            transportSpotsRemaining -= unit->getType().spaceRequired();
        }
    }

    // if there are still units to be added to the drop squad, do it
	if ((transportSpotsRemaining > 0 || !dropSquadHasTransport))
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

			//if ((unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || unit->getType() == BWAPI::UnitTypes::Terran_Vulture)
			//	&& _squadData.canAssignUnitToSquad(unit, dropSquad)
			//	//&& unit->getRegion()->getCenter() == InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion()->getCenter()
			//	&& dropSquadHasTransport
			//	)
			//{
			//	std::cout << std::endl  << " unit Id " << unit->getID() << " "  << unit->getType().c_str() << std::endl;
			//	std::cout << " InformationManager::Instance().getMapName() " << InformationManager::Instance().getMapName() << " 305 " << std::endl;
			//	std::cout << BWTA::getRegion(BWAPI::TilePosition(unit->getPosition()))->getCenter().x << " , " << BWTA::getRegion(BWAPI::TilePosition(unit->getPosition()))->getCenter().y
			//		<< " info : "
			//		<< InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion()->getCenter().x
			//		<< " , " << InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion()->getCenter().y << std::endl;
			//}
			//
            // get every unit of a lower priority and put it into the attack squad
			
			if (InformationManager::Instance().getMapName() == 'L')
			{
				if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode
					&& _squadData.canAssignUnitToSquad(unit, dropSquad)
					&& BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion()
					&& dropSquadHasTransport
					)
				{
					_squadData.assignUnitToSquad(unit, dropSquad);
					transportSpotsRemaining -= unit->getType().spaceRequired();
				}
			}
			else
			{
				if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture
					&& _squadData.canAssignUnitToSquad(unit, dropSquad)
					&& BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion() 
					&& dropSquadHasTransport
					)
				{
					_squadData.assignUnitToSquad(unit, dropSquad);
					transportSpotsRemaining -= unit->getType().spaceRequired();
				}
			}
        }
    }
    // otherwise the drop squad is full, so execute the order
	else if (dropShipUnit)
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
		int numDefendersPerEnemyUnit = 20;

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
				updateIdleSquad();
            }

            // and return, nothing to defend here
            continue;
        }
        else 
        {
			if (_combatStatus == InformationManager::combatStatus::rDefence)
				_combatStatus = InformationManager::combatStatus::nHelpDefence;
            // if we don't have a squad assigned to this region already, create one
            if (!_squadData.squadExists(squadName.str()))
            {
				std::cout << "Defend My Region!" << std::endl;
				SquadOrder defendRegion(SquadOrderTypes::Defend, regionCenter, 500, "Defend Region!");
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
			updateIdleSquad();
        }
    }
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded)
{

	//const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();
	const BWAPI::Unitset & squadUnits = _squadData.getSquad("Idle").getUnits();
    size_t flyingDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackAir);
    size_t groundDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackGround);
	//std::cout << "flyingDefendersInSquad " << flyingDefendersInSquad << " groundDefendersInSquad " << groundDefendersInSquad << std::endl;
    // if there's nothing left to defend, clear the squad
    if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
    {
        defenseSquad.clear();
		updateIdleSquad();
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

    //int zerglingsInOurBase = numZerglingsInOurBase();
    //bool zerglingRush = zerglingsInOurBase > 0 && BWAPI::Broodwar->getFrameCount() < 5000;

	for (auto & unit : _combatUnits) 
	{
		if (((flyingDefender && !UnitUtil::CanAttackAir(unit)) || (!flyingDefender && !UnitUtil::CanAttackGround(unit))) 
			&& unit->getType() != BWAPI::UnitTypes::Terran_Medic )
        {
            continue;
        }

        if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
        {
            continue;
        }

        // add workers to the defense squad if we are being rushed very quickly
        //if (!Config::Micro::WorkersDefendRush || (unit->getType().isWorker() && !zerglingRush && !beingBuildingRushed()))
        //{
        //    continue;
        //}

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
		if (!initMainAttackPath)
		{
			std::vector<BWAPI::TilePosition> tileList = BWTA::getShortestPath(BWAPI::TilePosition(InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter()), BWAPI::TilePosition(enemySecondCP->getCenter()));
			std::vector<std::pair<double, BWAPI::Position>> candidate_pos;
			for (auto & t : tileList) {
				BWAPI::Position tp(t.x * 32, t.y * 32);
				if (!tp.isValid())
					continue;
				if (tp.getDistance(InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter()) < 50)
					continue;
				if (tp.getDistance(InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->enemy())->getCenter()) < 200)
					continue;
				
				mainAttackPath.push_back(tp);
				
			}
			
			initMainAttackPath = true;
		}
		else{
			Squad & mainAttackSquad = _squadData.getSquad("MainAttack");
			if (curIndex < mainAttackPath.size())
			{
				//@도주남 김지훈 만약 공격 중이다가 우리의 인원수가 줄었다면 뒤로 뺀다 ?
				if (InformationManager::combatStatus::gEnemybase > InformationManager::Instance().nowCombatStatus)
				{
					curIndex = curIndex/2;
				}
				if (mainAttackPath[curIndex].getDistance(mainAttackSquad.calcCenter()) < mainAttackSquad.getUnits().size()*5)
					curIndex++;
				if (curIndex >= mainAttackPath.size())
					return mainAttackPath[mainAttackPath.size()-1];
				else
					return mainAttackPath[curIndex];
			}
		}
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

BWAPI::Position CombatCommander::getPositionForDefenceChokePoint(BWTA::Chokepoint * chokepoint)
{
	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	std::vector<BWAPI::TilePosition> tpList = BWTA::getShortestPath(BWAPI::TilePosition(ourBasePosition), BWAPI::TilePosition(chokepoint->getCenter()));
	BWAPI::Position resultPosition = ourBasePosition;
	for (auto & t : tpList) {
		BWAPI::Position tp(t.x * 32, t.y * 32);
		if (!tp.isValid())
			continue;
		if (tp.getDistance(chokepoint->getCenter()) <= BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 64
			&& tp.getDistance(chokepoint->getCenter()) >= BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange()+30)
		{
			resultPosition = tp;
		}
	}
	return resultPosition;
}

void CombatCommander::updateComBatStatusIndex()
{
	int totalUnits = _combatUnits.size();
	Squad & idleSquad = _squadData.getSquad("Idle");
	
	int idleUnitSize = idleSquad.getUnits().size();
	
	if (StrategyManager::Instance().getMainStrategy() == Strategy::main_strategies::Bionic)
	{
		if (idleUnitSize < 7 && _combatStatus <= InformationManager::combatStatus::wFirstChokePoint)
			_combatStatus = InformationManager::combatStatus::rDefence; // ready to Defence
		else if (idleUnitSize < 25)
			_combatStatus = InformationManager::combatStatus::wFirstChokePoint; // see first choke point
		else if (idleUnitSize < 40)
			_combatStatus = InformationManager::combatStatus::wSecondChokePoint; // see second choke point
		else if (idleUnitSize < 45)
			_combatStatus = InformationManager::combatStatus::rMainAttack; // ready to Attack
		else if (BWAPI::Broodwar->self()->supplyTotal() > 320)
		{
			_combatStatus = InformationManager::combatStatus::gEnemybase; // MainAttack
		}
		else if (_combatStatus >= InformationManager::combatStatus::wFirstChokePoint && totalUnits > 40)
			_combatStatus = InformationManager::combatStatus::jMainAttack; // add More Combat Unit For MainAttack
		else
			_combatStatus = InformationManager::combatStatus::idle;
	}
	else
	{
		int countTank = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);
		if (idleUnitSize < 7 && _combatStatus <= InformationManager::combatStatus::wFirstChokePoint)
			_combatStatus = InformationManager::combatStatus::rDefence; // ready to Defence
		else if (idleUnitSize <= 11 && countTank > 1)
			_combatStatus = InformationManager::combatStatus::wFirstChokePoint; // see first choke point
		else if (idleUnitSize <= 18 && countTank > 4)
			_combatStatus = InformationManager::combatStatus::wSecondChokePoint; // see second choke point
		else if (idleUnitSize <= 22 && countTank > 8)
			_combatStatus = InformationManager::combatStatus::rMainAttack; // ready to Attack
		else if (BWAPI::Broodwar->self()->supplyTotal() > 300)
		{
			_combatStatus = InformationManager::combatStatus::gEnemybase; // MainAttack
		}
		else if (_combatStatus >= InformationManager::combatStatus::wFirstChokePoint && BWAPI::Broodwar->self()->supplyTotal() > 240)
			_combatStatus = InformationManager::combatStatus::jMainAttack; // add More Combat Unit For MainAttack
		else
			_combatStatus = InformationManager::combatStatus::idle;
	}


	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Bunker) >0 
		&& _combatStatus == InformationManager::combatStatus::rDefence)
		_combatStatus = InformationManager::combatStatus::wFirstChokePoint; // see first choke point

	//if (InformationManager::Instance().nowCombatStatus > _combatStatus && _combatStatus <= InformationManager::combatStatus::wSecondChokePoint)
	//	_combatStatus = InformationManager::Instance().nowCombatStatus;
}


BWAPI::Position CombatCommander::getFirstChokePoint_OrderPosition()
{
	log_write("getFirstChokePoint_OrderPosition , ");
	BWTA::Chokepoint * startCP = InformationManager::Instance().getFirstChokePoint(BWAPI::Broodwar->self());
	BWTA::Chokepoint * endCP = InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self());
	if (indexFirstChokePoint_OrderPosition == -1)
	{
		std::vector<BWAPI::TilePosition> tileList = 
			BWTA::getShortestPath(BWAPI::TilePosition(startCP->getCenter())
			, BWAPI::TilePosition(endCP->getCenter()));
		for (auto & t : tileList) {
			BWAPI::Position tp(t.x * 32, t.y * 32);
			if (!tp.isValid())
				continue;
			firstChokePoint_OrderPositionPath.push_back(tp);
		}
		if (firstChokePoint_OrderPositionPath.size() > 0)
		{
			indexFirstChokePoint_OrderPosition = firstChokePoint_OrderPositionPath.size()/2;
			return firstChokePoint_OrderPositionPath[indexFirstChokePoint_OrderPosition];
		}
	}
	else{
		Squad & idleSquad = _squadData.getSquad("Idle");
		if (indexFirstChokePoint_OrderPosition < firstChokePoint_OrderPositionPath.size()-1)
		{
			if (firstChokePoint_OrderPositionPath[indexFirstChokePoint_OrderPosition].getDistance(idleSquad.calcCenter()) 
				< idleSquad.getUnits().size() * 8)
				indexFirstChokePoint_OrderPosition++;
			
			return firstChokePoint_OrderPositionPath[indexFirstChokePoint_OrderPosition];
		}
	}
	return endCP->getCenter();
}
