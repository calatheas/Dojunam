#include "CombatCommander.h"
#include "MedicManager.h"

using namespace MyBot;

const size_t IdlePriority = 0;
const size_t AttackPriority = 0;
const size_t BaseDefensePriority = 0;
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
	InformationManager & im = InformationManager::Instance();

	initMainAttackPath = false;
	curIndex = 0;

	indexFirstChokePoint_OrderPosition = -1;
	rDefence_OrderPosition = im.getMainBaseLocation(BWAPI::Broodwar->self())->getPosition();
	BWAPI::Position myFirstChokePointCenter = im.getFirstChokePoint(BWAPI::Broodwar->self())->getCenter();
	BWAPI::Unit targetDepot = nullptr;
	double maxDistance = 0;
	for (auto & munit : BWAPI::Broodwar->getAllUnits())
	{
		if (
			(munit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)
			&& myFirstChokePointCenter.getDistance(munit->getPosition()) >= maxDistance
			&& BWTA::getRegion(BWAPI::TilePosition(munit->getPosition())) 
			== im.getMainBaseLocation(BWAPI::Broodwar->self())->getRegion()
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
	wFirstChokePoint_OrderPosition = getPositionForDefenceChokePoint(im.getFirstChokePoint(BWAPI::Broodwar->self()));

	SquadOrder defcon1Order(SquadOrderTypes::Idle, rDefence_OrderPosition
		, rDefence_OrderPosition.getDistance(wFirstChokePoint_OrderPosition), "DEFCON1");
	_squadData.addSquad("DEFCON1", Squad("DEFCON1", defcon1Order, IdlePriority));


	int radiusAttack = im.getMapName() == 'H' ? 500 : 300;
	int radiusScout = 10;

	SquadOrder defcon2Order(SquadOrderTypes::Idle, wFirstChokePoint_OrderPosition
		, BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + radiusAttack, "DEFCON2");
	_squadData.addSquad("DEFCON2", Squad("DEFCON2", defcon2Order, IdlePriority));

	//앞마당 중간에서 정찰만
	SquadOrder defcon3Order(SquadOrderTypes::Idle, im.getFirstExpansionLocation(im.selfPlayer)->getRegion()->getCenter(), radiusScout, "DEFCON3");
	_squadData.addSquad("DEFCON3", Squad("DEFCON3", defcon3Order, IdlePriority));

	SquadOrder defcon4Order(SquadOrderTypes::Idle, im.getSecondChokePoint(im.selfPlayer)->getCenter(), radiusAttack, "DEFCON4");
	_squadData.addSquad("DEFCON4", Squad("DEFCON4", defcon4Order, IdlePriority));

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
	return BWAPI::Broodwar->getFrameCount() % 10 == 0;
}

void CombatCommander::update()
{
    if (!Config::Modules::UsingCombatCommander)
    {
        return;
    }

    if (!_initialized)
    {
        initializeSquads();
    }


	InformationManager & im = InformationManager::Instance();
	
	if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON1 ||
		im.nowCombatStatus == InformationManager::combatStatus::DEFCON2 ||
		im.nowCombatStatus == InformationManager::combatStatus::DEFCON3 ||
		im.nowCombatStatus == InformationManager::combatStatus::DEFCON4
		){

		if (im.changeConmgeStatusFrame == BWAPI::Broodwar->getFrameCount()){
			updateIdleSquad();
		}
		else{
			supplementSquad();
		}
		
	}
	else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON5){
		if (im.changeConmgeStatusFrame == BWAPI::Broodwar->getFrameCount()){
			updateDefenseSquads();
		}
		else{
			//추가병력 세팅
		}
	}
	else if (im.nowCombatStatus == InformationManager::combatStatus::CenterAttack ||
		im.nowCombatStatus == InformationManager::combatStatus::MainAttack ||
		im.nowCombatStatus == InformationManager::combatStatus::EnemyBaseAttack){
		if (im.changeConmgeStatusFrame == BWAPI::Broodwar->getFrameCount()){
			updateAttackSquads();
		}
		else{
			//추가병력 세팅
		}
	}

	//드롭스쿼드는 별도로 운영 : 우리 전장 상태와 무관
	if (isSquadUpdateFrame())
	{
        updateDropSquads();
	}
	
	_squadData.update();
	log_write("update END, ");
	drawSquadInformation(20, 200);
}

void CombatCommander::updateIdleSquad()
{
	InformationManager & im = InformationManager::Instance();

	Squad & defcon1Squad = _squadData.getSquad("DEFCON1");
	Squad & defcon2Squad = _squadData.getSquad("DEFCON2");
	Squad & defcon3Squad = _squadData.getSquad("DEFCON3");
	Squad & defcon4Squad = _squadData.getSquad("DEFCON4");

	std::cout << "updateIdleSquad:" << im.nowCombatStatus << std::endl;
    
	//BWAPI::Position mineralPosition = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getPosition();
	for (auto & unit : _combatUnits)
	{
		//전투 준비
		if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON1)
		{
			
			if (_squadData.canAssignUnitToSquad(unit, defcon1Squad))
			{
				//idleSquad.addUnit(unit);
				std::cout << "D1,";
				_squadData.assignUnitToSquad(unit, defcon1Squad);
			}
		}

		//첫번째 초크
		else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON2)
		{
			if (_squadData.canAssignUnitToSquad(unit, defcon2Squad))
			{
				//idleSquad.addUnit(unit);
				std::cout << "D2,";
				_squadData.assignUnitToSquad(unit, defcon2Squad);
			}
		}

		//첫번째 초크 밖
		else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON3)
		{
			//첫번째 초크 안쪽에 전부 배치하고
			if (_squadData.canAssignUnitToSquad(unit, defcon2Squad))
			{
				//idleSquad.addUnit(unit);
				std::cout << "D2,";
				_squadData.assignUnitToSquad(unit, defcon2Squad);
			}

			//1마리만 밖에서 정찰 (탱크는 제외)
			if (defcon3Squad.getUnits().size() == 0 && 
				(unit->getType() != BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || unit->getType() != BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) &&
				_squadData.canAssignUnitToSquad(unit, defcon3Squad))
			{
				//idleSquad.addUnit(unit);
				std::cout << "D3,";
				_squadData.assignUnitToSquad(unit, defcon3Squad);
			}
		}

		//두번째 쵸크
		else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON4)
		{
			if (_squadData.canAssignUnitToSquad(unit, defcon4Squad))
			{
				//idleSquad.addUnit(unit);
				std::cout << "D4,";
				_squadData.assignUnitToSquad(unit, defcon4Squad);
			}
		}
		//else if (_combatStatus == InformationManager::combatStatus::CenterAttack){
		//	SquadOrder idleOrder(SquadOrderTypes::Idle,
		//		getIdleSquadLastOrderLocation()
		//		, radi, "Move Out");
		//	idleSquad.setSquadOrder(idleOrder);
		//}

		log_write("Idlesquad END, ");
	}
	std::cout << std::endl;
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

	for (auto & unit : _combatUnits)
	{
		//if (_dropUnits.contains(unit))
		//	continue;
		if (_squadData.canAssignUnitToSquad(unit, mainAttackSquad))
		{
			_squadData.assignUnitToSquad(unit, mainAttackSquad);
		}
	}

	InformationManager & im = InformationManager::Instance();
	int radi = 400;
	if (im.nowCombatStatus == InformationManager::combatStatus::CenterAttack)
	{
		BWAPI::Position mCenter((BWAPI::Broodwar->mapWidth() * 16), BWAPI::Broodwar->mapHeight() * 16);
		SquadOrder _order(SquadOrderTypes::Attack, mCenter, radi, "Attack Center");
		mainAttackSquad.setSquadOrder(_order);
	}
	else if (im.nowCombatStatus == InformationManager::combatStatus::EnemyBaseAttack)
	{
		SquadOrder _order(SquadOrderTypes::Attack, im.getMainBaseLocation(im.enemyPlayer)->getPosition(), radi, "Attack Enemy base");
		mainAttackSquad.setSquadOrder(_order);
	}
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

	int numDefendersPerEnemyUnit = 20;

	for(auto r : defenceRegions)
	{
		std::stringstream squadName;
		squadName << "Base Defense " << r->getCenter().x << " " << r->getCenter().y;

		// if we don't have a squad assigned to this region already, create one
		if (!_squadData.squadExists(squadName.str()))
		{
			SquadOrder defendRegion(SquadOrderTypes::Defend, r->getCenter(), 500, "Defend Region!");
			_squadData.addSquad(squadName.str(), Squad(squadName.str(), defendRegion, BaseDefensePriority));
		}

		Squad & defenseSquad = _squadData.getSquad(squadName.str());

		// figure out how many units we need on defense
		int flyingDefendersNeeded = numDefendersPerEnemyUnit * unitNumInDefenceRegion[r].first;
		int groundDefensersNeeded = numDefendersPerEnemyUnit * unitNumInDefenceRegion[r].second;

		updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefensersNeeded);
	}
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded)
{
	const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();
    size_t flyingDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackAir);
    size_t groundDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackGround);
	//std::cout << "flyingDefendersInSquad " << flyingDefendersInSquad << " groundDefendersInSquad " << groundDefendersInSquad << std::endl;

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
				if (InformationManager::combatStatus::EnemyBaseAttack > InformationManager::Instance().nowCombatStatus)
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

void CombatCommander::updateComBatStatus(const BWAPI::Unitset & combatUnits)
{
	InformationManager::combatStatus _combatStatus = InformationManager::combatStatus::Idle;
	_combatUnits = combatUnits;
	
	int totalUnits = _combatUnits.size();
	InformationManager &im = InformationManager::Instance();
	
	if (StrategyManager::Instance().getMainStrategy() == Strategy::main_strategies::Bionic)
	{
		if (totalUnits < 7)
			_combatStatus = InformationManager::combatStatus::DEFCON1; //방어준비
		else{
			_combatStatus = InformationManager::combatStatus::DEFCON2; //첫번째 쵸크 안쪽 이동

			if (_combatStatus == InformationManager::combatStatus::DEFCON2 && im.checkFirstRush()){
				_combatStatus = InformationManager::combatStatus::DEFCON3; // 첫번째 초크 밖 이동

				//적 발견시
				//쵸크 안쪽에서 싸우기
				//일꾼 동원하기
				//for (auto u : BWAPI::Broodwar->enemy()->getUnits()){
				//	if (!u->getType().isBuilding()){
				//		if (BWTA::getRegion(im.getFirstExpansionLocation(im.selfPlayer)->getPosition())->getPolygon().isInside(u->getPosition())){
				//			_combatStatus = InformationManager::combatStatus::DEFCON2; // 첫번째 쵸크 안쪽 이동

				//		}
				//	}
				//}
			}

			if (totalUnits > 24)
				_combatStatus = InformationManager::combatStatus::DEFCON4; // 두번째 초크 이동

			if (totalUnits > 45)
				_combatStatus = InformationManager::combatStatus::CenterAttack; // 취약 지역 공격
		}


		if (BWAPI::Broodwar->self()->supplyTotal() > 320)
		{
			_combatStatus = InformationManager::combatStatus::EnemyBaseAttack; // 적 본진 공격
		}
	}
	else
	{
		int countTank = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);
		if (totalUnits < 7)
			_combatStatus = InformationManager::combatStatus::DEFCON1; // 방어준비
		else{
			_combatStatus = InformationManager::combatStatus::DEFCON1; // 방어준비

			if (countTank > 1)
				_combatStatus = InformationManager::combatStatus::DEFCON2; // 첫번째 쵸크 안쪽 이동

			if (_combatStatus == InformationManager::combatStatus::DEFCON2 && im.checkFirstRush()){
				_combatStatus = InformationManager::combatStatus::DEFCON3; // 첫번째 초크 밖 이동
				
				//적 발견시
				//쵸크 안쪽에서 싸우기
				//일꾼 동원하기
				//for (auto u : BWAPI::Broodwar->enemy()->getUnits()){
				//	if (!u->getType().isBuilding()){
				//		if (BWTA::getRegion(im.getFirstExpansionLocation(im.selfPlayer)->getPosition())->getPolygon().isInside(u->getPosition())){
				//			_combatStatus = InformationManager::combatStatus::DEFCON2; // 첫번째 쵸크 안쪽 이동
				//			
				//		}
				//	}
				//}
			}

			if (countTank > 4)
				_combatStatus = InformationManager::combatStatus::DEFCON4; // 두번째 초크 이동
			if (countTank > 8)
				_combatStatus = InformationManager::combatStatus::CenterAttack; // 취약 지역 공격
		}

		if (BWAPI::Broodwar->self()->supplyTotal() > 300)
		{
			_combatStatus = InformationManager::combatStatus::EnemyBaseAttack; // 적 본진 공격
		}
	}

	//디펜스 상황 판단
	std::vector<BWTA::Region *> tmp_defenceRegions;
	std::map<BWTA::Region *, std::pair<int, int>> tmp_unitNumInDefenceRegion;

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
		if (myRegion == enemyRegion) continue;

		BWAPI::Position regionCenter = myRegion->getCenter();
		if (!regionCenter.isValid()) continue;


		// all of the enemy units in this region
		BWAPI::Unitset enemyUnitsInRegion;
		for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			// if it's an overlord, don't worry about it for defense, we don't care what they see
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) continue;

			if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
			{
				//std::cout << "enemyUnits In My Region " << std::endl;
				enemyUnitsInRegion.insert(unit);
			}
		}

		// if there's nothing in this region to worry about
		if (enemyUnitsInRegion.empty())
		{
			continue;
		}
		else
		{
			// we can ignore the first enemy worker in our region since we assume it is a scout
			int numEnemyFlyingInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return u->isFlying(); });
			int numEnemyGroundInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return !u->isFlying(); });

			tmp_defenceRegions.push_back(myRegion);
			tmp_unitNumInDefenceRegion[myRegion] = std::make_pair(numEnemyFlyingInRegion, numEnemyGroundInRegion);
		}
	}

	if (tmp_defenceRegions.size() > 0){
		_combatStatus = InformationManager::combatStatus::DEFCON5; // 본진 방어
		defenceRegions = tmp_defenceRegions;
		unitNumInDefenceRegion = tmp_unitNumInDefenceRegion;
	}

	InformationManager::Instance().setCombatStatus(_combatStatus);
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

void CombatCommander::supplementSquad(){
	InformationManager & im = InformationManager::Instance();

	Squad & defcon1Squad = _squadData.getSquad("DEFCON1");
	Squad & defcon2Squad = _squadData.getSquad("DEFCON2");
	Squad & defcon3Squad = _squadData.getSquad("DEFCON3");
	Squad & defcon4Squad = _squadData.getSquad("DEFCON4");

	//추가병력만 세팅
	for (auto & unit : _combatUnits){
		if (_squadData.getUnitSquad(unit) == nullptr){
			if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON1){
				_squadData.assignUnitToSquad(unit, defcon1Squad);
			}
			else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON2){
				_squadData.assignUnitToSquad(unit, defcon2Squad);
			}
			else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON3){
				_squadData.assignUnitToSquad(unit, defcon2Squad);
			}
			else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON4){
				_squadData.assignUnitToSquad(unit, defcon4Squad);
			}
		}
	}
}