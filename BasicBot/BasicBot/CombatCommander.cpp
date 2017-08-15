﻿#include "CombatCommander.h"
#include "MedicManager.h"

using namespace MyBot;

const size_t IdlePriority = 0;
const size_t AttackPriority = 0;
const size_t BaseDefensePriority = 0;
const size_t ScoutPriority = 1;
const size_t BunkerPriority = 2;
//const size_t ScoutDefensePriority = 3;
const size_t DropPriority = 4;

CombatCommander & CombatCommander::Instance()
{
	static CombatCommander instance;
	return instance;
}

CombatCommander::CombatCommander() 
    : _initialized(false),
	notDraw(false)
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
	//std::cout << "targetDepot " << targetDepot->getPosition().x/ 32 << " " << targetDepot->getPosition().y/32 << std::endl;
	dx = (targetDepot->getPosition().x - rDefence_OrderPosition.x)*0.7f;
	dy = (targetDepot->getPosition().y - rDefence_OrderPosition.y)*0.7f;
	rDefence_OrderPosition = rDefence_OrderPosition + BWAPI::Position(dx, dy);// (mineralPosition + closestDepot->getPosition()) / 2;
	wFirstChokePoint_OrderPosition = getPositionForDefenceChokePoint(im.getFirstChokePoint(BWAPI::Broodwar->self()));

	/*
	방어형 쵸크포인트 계산
	첫번째쵸크 10% 뒤에 있고 우리 리젼으로 확장 
	*/
	BWAPI::Position tmpBase(BWAPI::Broodwar->self()->getStartLocation());
	BWAPI::Position tmpChoke(im.getFirstChokePoint(BWAPI::Broodwar->self())->getCenter());
	
	double tmpRatio = 0.1;
	std::pair<int, int> vecBase2Choke;
	vecBase2Choke.first = tmpBase.x - tmpChoke.x;
	vecBase2Choke.second = tmpBase.y - tmpChoke.y;
	BWAPI::Position in_1st_chock_center(tmpChoke.x + (int)(vecBase2Choke.first*tmpRatio), tmpChoke.y + (int)(vecBase2Choke.second*tmpRatio));
	std::pair<BWAPI::Position, BWAPI::Position> in_1st_chock_side;
	in_1st_chock_side.first = BWAPI::Position(im.getFirstChokePoint(BWAPI::Broodwar->self())->getSides().first.x + (int)(vecBase2Choke.first*tmpRatio), im.getFirstChokePoint(BWAPI::Broodwar->self())->getSides().first.y + (int)(vecBase2Choke.second*tmpRatio));
	in_1st_chock_side.second = BWAPI::Position(im.getFirstChokePoint(BWAPI::Broodwar->self())->getSides().second.x + (int)(vecBase2Choke.first*tmpRatio), im.getFirstChokePoint(BWAPI::Broodwar->self())->getSides().second.y + (int)(vecBase2Choke.second*tmpRatio));
	
	std::cout << "base:" << tmpBase << std::endl;
	std::cout << "choke:" << tmpChoke << std::endl;
	std::cout << "vecBase2Choke:" << vecBase2Choke.first << "," << vecBase2Choke.second << std::endl;
	std::cout << "in_1st_chock_center:" << in_1st_chock_center.x / 32 << "," << in_1st_chock_center.y / 32 << std::endl;
	std::cout << "in_1st_chock_side:" << in_1st_chock_side.first.x / 32 << "," << in_1st_chock_side.first.y / 32 << "/" << in_1st_chock_side.second.x / 32 << "," << in_1st_chock_side.second.y / 32 << std::endl;
	

	SquadOrder defcon1Order(SquadOrderTypes::Idle, rDefence_OrderPosition
		, rDefence_OrderPosition.getDistance(wFirstChokePoint_OrderPosition), "DEFCON1");
	_squadData.addSquad("DEFCON1", Squad("DEFCON1", defcon1Order, IdlePriority));


	int radiusAttack = im.getMapName() == 'H' ? 50 : 30;
	int radiusScout = 10;

	SquadOrder defcon2Order(SquadOrderTypes::Idle, in_1st_chock_center
		, BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + radiusAttack, in_1st_chock_side, "DEFCON2");
	_squadData.addSquad("DEFCON2", Squad("DEFCON2", defcon2Order, IdlePriority));

	//앞마당 중간에서 정찰만
	std::vector<BWAPI::TilePosition> expansion2choke = BWTA::getShortestPath(BWAPI::TilePosition(im.getFirstChokePoint(im.selfPlayer)->getCenter()), BWAPI::TilePosition(im.getSecondChokePoint(im.selfPlayer)->getCenter()));
	SquadOrder defcon3Order(SquadOrderTypes::Idle, BWAPI::Position(expansion2choke[expansion2choke.size() / 2]), radiusScout, "DEFCON3");
	_squadData.addSquad("DEFCON3", Squad("DEFCON3", defcon3Order, IdlePriority));

	SquadOrder defcon4Order(SquadOrderTypes::Idle, im.getSecondChokePoint(im.selfPlayer)->getCenter(), 
		BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + radiusAttack, im.getSecondChokePoint(im.selfPlayer)->getSides(), "DEFCON4");
	_squadData.addSquad("DEFCON4", Squad("DEFCON4", defcon4Order, IdlePriority));
	
	SquadOrder scoutOrder(SquadOrderTypes::Idle, im.getFirstChokePoint(im.selfPlayer)->getSides().first, 70, "scout");
	_squadData.addSquad("scout", Squad("scout", scoutOrder, ScoutPriority));

    // the main attack squad that will pressure the enemy's closest base location
	SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocationForCombat(), 800, "Attack Enemy Base");
	_squadData.addSquad("MainAttack", Squad("MainAttack", mainAttackOrder, AttackPriority));

	SquadOrder smallAttackOrder(SquadOrderTypes::Attack, getMainAttackLocationForCombat(), 250, "Attack Enemy multi");
	_squadData.addSquad("SMALLATTACK", Squad("SMALLATTACK", smallAttackOrder, AttackPriority));

    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

	SquadOrder zealotDrop(SquadOrderTypes::Drop, ourBasePosition, 410, "Wait for transport");
    _squadData.addSquad("Drop", Squad("Drop", zealotDrop, DropPriority));    

    _initialized = true;
}

bool CombatCommander::isSquadUpdateFrame()
{
	return BWAPI::Broodwar->getFrameCount() % 10 == 0;
}

void CombatCommander::update()
{
    //if (!Config::Modules::UsingCombatCommander)
    //{
    //    return;
    //}

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
			supplementSquad();
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
			supplementSquad();
		}
	}

	//드롭스쿼드는 별도로 운영 : 우리 전장 상태와 무관
	if (isSquadUpdateFrame())
	{
		if (im.nowCombatStatus < InformationManager::combatStatus::DEFCON4)//초반에만 벙커를 설치한다고 가정하고,
			updateBunkertSquads();
        updateDropSquads();
		updateScoutSquads();
		if (im.nowCombatStatus <= InformationManager::combatStatus::DEFCON4)
			saveAllSquadSecondChokePoint();
		updateSmallAttackSquad();
		//어택포지션 변경
		if (im.nowCombatStatus == InformationManager::combatStatus::EnemyBaseAttack){
			Squad & mainAttackSquad = _squadData.getSquad("MainAttack");
			SquadOrder _order(SquadOrderTypes::Attack, getMainAttackLocationForCombat(), mainAttackSquad.getSquadOrder().getRadius(), "Attack Enemy base");
			mainAttackSquad.setSquadOrder(_order);
		}
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

	std::pair<int, BWAPI::Unit> shortDistanceUnitForDEFCON3;
	shortDistanceUnitForDEFCON3.first = 1000000;

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
				_squadData.assignUnitToSquad(unit, defcon1Squad);
			}
		}

		//첫번째 초크
		else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON2)
		{
			if (_squadData.canAssignUnitToSquad(unit, defcon2Squad))
			{
				//idleSquad.addUnit(unit);
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
				_squadData.assignUnitToSquad(unit, defcon2Squad);

				if ((unit->getType() != BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || unit->getType() != BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)){
					int tmpDistance = unit->getPosition().getDistance(im.getFirstExpansionLocation(im.selfPlayer)->getPosition());
					if (tmpDistance < shortDistanceUnitForDEFCON3.first){
						std::cout << "min:" << shortDistanceUnitForDEFCON3.first << std::endl;
						shortDistanceUnitForDEFCON3.first = tmpDistance;
						shortDistanceUnitForDEFCON3.second = unit;
					}
				}
			}
		}

		//두번째 쵸크
		else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON4)
		{
			if (_squadData.canAssignUnitToSquad(unit, defcon4Squad))
			{
				//idleSquad.addUnit(unit);
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

	//1마리만 밖에서 정찰 (탱크는 제외)
	if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON3){
		if (shortDistanceUnitForDEFCON3.first < 1000000)
		{
			std::cout << "set D3" << std::endl;
			_squadData.assignUnitToSquad(shortDistanceUnitForDEFCON3.second, defcon3Squad);
		}
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
	int radi = 410;
	if (im.nowCombatStatus == InformationManager::combatStatus::CenterAttack)
	{
		std::vector<BWAPI::TilePosition> tileList = BWTA::getShortestPath(BWAPI::TilePosition(im.getSecondChokePoint(BWAPI::Broodwar->self())->getCenter()), BWAPI::TilePosition(im.getSecondChokePoint(im.enemyPlayer)->getCenter()));
		SquadOrder _order(SquadOrderTypes::Attack, BWAPI::Position(tileList[tileList.size() / 2]), radi, "Attack Center");
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


	if (_squadData.squadExists("Drop") && dropUnits.size() > 0)
	{
		SquadOrder goingDrop(SquadOrderTypes::Drop, InformationManager::Instance().getDropPosition(), 410, "Go Drop");
		dropSquad.setSquadOrder(goingDrop);
	}

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

            // get every unit of a lower priority and put it into the attack squad
			
			if (InformationManager::Instance().getMapName() == 'L')
			{
				if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode
					&& _squadData.canAssignUnitToSquad(unit, dropSquad)
					//&& BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion()
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
					//&& BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getRegion() 
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
		squadName << "DEFCON5" << r->getCenter().x << "_" << r->getCenter().y;

		// if we don't have a squad assigned to this region already, create one
		if (!_squadData.squadExists(squadName.str()))
		{
			SquadOrder defendRegion(SquadOrderTypes::Defend, unitNumInDefenceRegion[r].getPosition(), 500, "Defend Region!");
			_squadData.addSquad(squadName.str(), Squad(squadName.str(), defendRegion, BaseDefensePriority));
		}

		Squad & defenseSquad = _squadData.getSquad(squadName.str());

		int flyingDefendersNeeded = std::count_if(unitNumInDefenceRegion[r].begin(), unitNumInDefenceRegion[r].end(), [](BWAPI::Unit u) { return u->isFlying(); });
		int groundDefensersNeeded = std::count_if(unitNumInDefenceRegion[r].begin(), unitNumInDefenceRegion[r].end(), [](BWAPI::Unit u) { return !u->isFlying(); });
		// figure out how many units we need on defense
		flyingDefendersNeeded *= numDefendersPerEnemyUnit;
		groundDefensersNeeded *= numDefendersPerEnemyUnit;

		updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefensersNeeded);
	}
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded)
{
	const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();
    size_t flyingDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtils::CanAttackAir);
    size_t groundDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtils::CanAttackGround);
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
		if (((flyingDefender && !UnitUtils::CanAttackAir(unit)) || (!flyingDefender && !UnitUtils::CanAttackGround(unit))) 
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

BWAPI::Position CombatCommander::getMainAttackLocationForCombat()
{
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
	BWTA::Chokepoint * enemySecondCP = InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->enemy());
	
	if (enemySecondCP)
	{
		if (!initMainAttackPath)
		{
			std::vector<BWAPI::TilePosition> tileList 
				= BWTA::getShortestPath(BWAPI::TilePosition(InformationManager::Instance().getSecondChokePoint(BWAPI::Broodwar->self())->getCenter())
										, BWAPI::TilePosition(enemySecondCP->getCenter()));
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
			if (curIndex < mainAttackPath.size() / 2)
				curIndex = mainAttackPath.size() / 2;
			if (curIndex < mainAttackPath.size())
			{
				//@도주남 김지훈 만약 공격 중이다가 우리의 인원수가 줄었다면 뒤로 뺀다 ?
				if (mainAttackPath[curIndex].getDistance(mainAttackSquad.calcCenter()) < mainAttackSquad.getUnits().size()*9)
					curIndex+=3;
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

		if (UnitUtils::IsValidUnit(unit) && unit->isVisible())
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

			if (_combatStatus == InformationManager::combatStatus::DEFCON2 && (im.rushState == 0 && im.getRushSquad().size() > 0)){
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
		int countTank = UnitUtils::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) + UnitUtils::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);
		int maxDEFCON1 = im.enemyRace == BWAPI::Races::Zerg ? 6 : 2;
		if (totalUnits < maxDEFCON1)
			_combatStatus = InformationManager::combatStatus::DEFCON1; // 방어준비
		else{
			_combatStatus = InformationManager::combatStatus::DEFCON2; // 첫번째 쵸크 안쪽 이동

			if (im.rushState == 0 && im.getRushSquad().size() > 0){
				//적 발견시
				//쵸크 안쪽에서 싸우기
				//일꾼 동원하기
				_combatStatus = InformationManager::combatStatus::DEFCON3; // 첫번째 초크 밖 이동

			}
			else{
				int expansionStatus = ExpansionManager::Instance().shouldExpandNow();
				
				//적 멀티 발견시, 적 입구 막을때는 무조건 멀티 시작
				//그렇지 않으면 시즈는 완성되고 간다.
				if (expansionStatus == 2){
					_combatStatus = InformationManager::combatStatus::DEFCON4; // 두번째 초크 이동
				}
				else if (expansionStatus > 0){
					if (countTank > 0 && StrategyManager::Instance().hasTech(BWAPI::TechTypes::Tank_Siege_Mode)){
						_combatStatus = InformationManager::combatStatus::DEFCON4; // 두번째 초크 이동
					}
				}
			}

			if (countTank > 3)
				_combatStatus = InformationManager::combatStatus::DEFCON4; // 두번째 초크 이동
			if (countTank > 7)
				_combatStatus = InformationManager::combatStatus::CenterAttack; // 취약 지역 공격
		}

		if (BWAPI::Broodwar->self()->supplyTotal() > 300)
		{
			_combatStatus = InformationManager::combatStatus::EnemyBaseAttack; // 적 본진 공격
		}
	}

	//디펜스 상황 판단
	std::vector<BWTA::Region *> tmp_defenceRegions;
	std::map<BWTA::Region *, BWAPI::Unitset> tmp_unitNumInDefenceRegion;

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
		int ignoreNumWorker = 2;
		for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (unit->getType().isWorker() && ignoreNumWorker > 0){
				ignoreNumWorker--;
				continue;
			}

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
			//int numEnemyFlyingInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return u->isFlying(); });
			//int numEnemyGroundInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return !u->isFlying(); });

			tmp_defenceRegions.push_back(myRegion);
			tmp_unitNumInDefenceRegion[myRegion] = enemyUnitsInRegion;
		}
	}

	if (tmp_defenceRegions.size() > 0){
		_combatStatus = InformationManager::combatStatus::DEFCON5; // 본진 방어
		defenceRegions = tmp_defenceRegions;
		unitNumInDefenceRegion = tmp_unitNumInDefenceRegion;
	}

	InformationManager::Instance().setCombatStatus(_combatStatus);

	if (BWAPI::Broodwar->getFrameCount() >= 16800 && InformationManager::Instance().getUnitAndUnitInfoMap(InformationManager::Instance().enemyPlayer).size() == 0){
		notDraw = true;
	}
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
	Squad & mainAttackSquad = _squadData.getSquad("MainAttack");

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
			else if (im.nowCombatStatus == InformationManager::combatStatus::DEFCON5){
				//디펜스지역 중 가장 가까운 지역으로 이동
				std::pair<int, BWAPI::Position> minDistRegion;
				minDistRegion.first = 100000000;
				for (auto r : defenceRegions)
				{
					int tmpDist = unit->getPosition().getDistance(r->getCenter());
					if (tmpDist < minDistRegion.first){
						minDistRegion.first = tmpDist;
						minDistRegion.second = BWAPI::Position(r->getCenter());
					}
				}

				if (minDistRegion.first < 100000000){
					std::stringstream squadName;
					squadName << "DEFCON5" << minDistRegion.second.x << "_" << minDistRegion.second.y;

					if (_squadData.squadExists(squadName.str()))
					{
						_squadData.assignUnitToSquad(unit, _squadData.getSquad(squadName.str()));
					}
				}
			}
			else if (im.nowCombatStatus == InformationManager::combatStatus::CenterAttack){
				_squadData.assignUnitToSquad(unit, mainAttackSquad);
			}
			else if (im.nowCombatStatus == InformationManager::combatStatus::EnemyBaseAttack){
				_squadData.assignUnitToSquad(unit, defcon4Squad);
			}
		}
	}
}

void CombatCommander::updateScoutSquads()
{
	if (!BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Spider_Mines))
		return;

	Squad & scoutSquad = _squadData.getSquad("scout");
	if (scoutSquad.getUnits().size() >= 1)
		return;
	else
	{
		for (auto & unit : _combatUnits){
			if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture && scoutSquad.getUnits().size() < 1)
			{
				if (_squadData.canAssignUnitToSquad(unit, scoutSquad))
					_squadData.assignUnitToSquad(unit, scoutSquad);
			}
		}
	}
}

void CombatCommander::updateBunkertSquads()
{
	int bunkerNum = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Bunker);
	if (bunkerNum == 0)
	{
		if (_squadData.squadExists("bunker"))
		{
			Squad & bunkerSquad = _squadData.getSquad("bunker");
			bunkerSquad.clear();
		}
		return;
	}
	BWAPI::Unit bunker =
		UnitUtils::GetFarUnitTypeToTarget(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));

	SquadOrder bunkerOrder(SquadOrderTypes::Idle, bunker->getPosition(), 300, std::make_pair(bunker->getPosition(), bunker->getPosition()), "bunker");
	if (!_squadData.squadExists("bunker"))
		_squadData.addSquad("bunker", Squad("bunker", bunkerOrder, BunkerPriority));

	Squad & bunkerSquad = _squadData.getSquad("bunker");
	bunkerSquad.setSquadOrder(bunkerOrder);


	Squad & defcon1Squad = _squadData.getSquad("DEFCON1");
	Squad & defcon2Squad = _squadData.getSquad("DEFCON2");
	Squad & defcon3Squad = _squadData.getSquad("DEFCON3");
	Squad & defcon4Squad = _squadData.getSquad("DEFCON4");


	SquadOrder saveBunker(SquadOrderTypes::Idle, bunker->getPosition()
		, BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 10, "SAVEBUNKER");
	
	defcon1Squad.setSquadOrder(saveBunker);
	defcon2Squad.setSquadOrder(saveBunker);
	defcon3Squad.setSquadOrder(saveBunker);
	defcon4Squad.setSquadOrder(saveBunker);

	if (bunkerSquad.getUnits().size() >= 4)
		return;
	else
	{
		for (auto & unit : _combatUnits){
			if (unit->getType() == BWAPI::UnitTypes::Terran_Marine && bunkerSquad.getUnits().size() < 4)
			{
				if(_squadData.canAssignUnitToSquad(unit, bunkerSquad))
					_squadData.assignUnitToSquad(unit, bunkerSquad);
			}
		}
	}
}

void CombatCommander::saveAllSquadSecondChokePoint()
{
	Squad & defcon1Squad = _squadData.getSquad("DEFCON1");
	Squad & defcon2Squad = _squadData.getSquad("DEFCON2");
	Squad & defcon3Squad = _squadData.getSquad("DEFCON3");
	Squad & defcon4Squad = _squadData.getSquad("DEFCON4");

	bool goCommandCenter = false;
	for (auto & isCommandCenter : BWAPI::Broodwar->getUnitsOnTile(InformationManager::Instance().getFirstExpansionLocation(BWAPI::Broodwar->self())->getTilePosition()))
	{
		if (isCommandCenter->getType() == BWAPI::UnitTypes::Terran_Command_Center)
		{
			goCommandCenter = true;
			break;
		}
	}

	if (goCommandCenter)
	{

		InformationManager & im = InformationManager::Instance();

		SquadOrder defcon4Order(SquadOrderTypes::Idle, im.getSecondChokePoint(im.selfPlayer)->getCenter(),
			BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 30, im.getSecondChokePoint(im.selfPlayer)->getSides(), "DEFCON4");

		defcon1Squad.setSquadOrder(defcon4Order);
		defcon2Squad.setSquadOrder(defcon4Order);
		defcon3Squad.setSquadOrder(defcon4Order);
		defcon4Squad.setSquadOrder(defcon4Order);
	}
	else
		return;
}

void CombatCommander::updateSmallAttackSquad()
{
	std::list<BWTA::BaseLocation *> enemayBaseLocations = InformationManager::Instance().getOccupiedBaseLocations(BWAPI::Broodwar->enemy());
	
	BWTA::BaseLocation * eBase = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
	BWTA::BaseLocation * eFirstMulti = InformationManager::Instance().getFirstExpansionLocation(BWAPI::Broodwar->enemy());
	BWTA::BaseLocation * attackBaseLocation = nullptr;
	for (BWTA::BaseLocation * startLocation : enemayBaseLocations)
	{
		if (startLocation == eBase || startLocation == eFirstMulti)
			continue;
		attackBaseLocation = startLocation;
	}

	if (attackBaseLocation == nullptr)
	{
		if (_squadData.squadExists("SMALLATTACK"))
		{
			Squad & smallAttackSquad = _squadData.getSquad("SMALLATTACK");
			smallAttackSquad.clear();
		}
		return;
	}

	Squad & smallAttackSquad = _squadData.getSquad("SMALLATTACK");
	if (smallAttackSquad.getUnits().size() > 6 && attackBaseLocation->getPosition() == smallAttackSquad.getSquadOrder().getPosition())
		return;

	for (auto & unit : _combatUnits)
	{
		if(smallAttackSquad.getUnits().size() > 6)
			break;
		if (_squadData.canAssignUnitToSquad(unit, smallAttackSquad) && unit->getType() == BWAPI::UnitTypes::Terran_Vulture)
		{
			_squadData.assignUnitToSquad(unit, smallAttackSquad);
		}
	}

	SquadOrder smallAttackOrder(SquadOrderTypes::Attack, attackBaseLocation->getPosition(), 250, "Attack Enemy multi");
	smallAttackSquad.setSquadOrder(smallAttackOrder);
}