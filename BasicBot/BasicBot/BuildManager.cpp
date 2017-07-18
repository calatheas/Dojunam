#include "BuildManager.h"

using namespace MyBot;

BuildManager::BuildManager() 
	: _enemyCloakedDetected (false)
{
	setBuildOrder(StrategyManager::Instance().getOpeningBookBuildOrder());
}

void BuildManager::onStart(){
	if (InformationManager::Instance().getMapName() == 'H'){
		maxDepotWorkers = 10; //미네랄10덩어리
	}
	else if (InformationManager::Instance().getMapName() == 'H'){
		maxDepotWorkers = 9; //미네랄9덩어리
	}
	else{
		maxDepotWorkers = 8; //미네랄8덩어리
	}

}

// 빌드오더 큐에 있는 것에 대해 생산 / 건설 / 리서치 / 업그레이드를 실행한다
void BuildManager::update()
{
	/* 개요
	1. 빌드큐에서 우선순위 높은 빌드를 가지고 온다. 처리가능 하면(생산자, 미네랄, 위치 등) 처리. 처리 못하면 스킵 하거나 다음 프레임에서 처리
	1.1 건물의 경우는 건물큐로 일단 보냄(아주 간단한 정보만 확인 후). 컨스트럭트 매니저에서 데드락 관리
	2. 처리 못하는 경우는 미네랄이 부족한 경우만 있다라고 가정된 상황(필요한 건물등은 모두 만들어져 있다고 가정)
	3. 서플라이 부족한 경우는 데드락 발생하므로 예외처리 되어 있음
	4. (todo) 추가적으로 예외 상황에서 못만드는 경우가 있으므로 데드락 처리가 필요
	5. 빌드 큐가 모두 소진되면 BOSS를 통해 새로운 빌드오더 생성
	6. (todo) 디텍팅 유닛이 필요한 경우, 디텍 유닛 세팅 : 조금더 고민이 필요, 터렛 설치 장소 등
	7. (todo) 디펜스 상황에서 유닛을 빼거나 전투유닛만 만들어야 되는 상황
	*/

	/*
	// Dead Lock 을 체크해서 제거한다
	//checkBuildOrderQueueDeadlockAndAndFixIt();
	// Dead Lock 제거후 Empty 될 수 있다
	if (buildQueue.isEmpty()) {
		return;
	}
	*/

	consumeBuildQueue(); //빌드오더 소비

	//큐가 비어 있으면 새로운 빌드오더 생성
	if (buildQueue.isEmpty()) {
		if ((buildQueue.size() == 0) && (BWAPI::Broodwar->getFrameCount() > 10))
		{
			BWAPI::Broodwar->drawTextScreen(150, 10, "Nothing left to build, new search!");

			performBuildOrderSearch();
		}
	}

	// 서플라이 데드락 체크
	// detect if there's a build order deadlock once per second
	if ((BWAPI::Broodwar->getFrameCount() % 24 == 0) && detectSupplyDeadlock())
	{
		std::cout << "Supply deadlock detected, building supply!" << std::endl;

		buildQueue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Terran_Supply_Depot), StrategyManager::Instance().getBuildSeedPositionStrategy(MetaType(BWAPI::UnitTypes::Terran_Supply_Depot)), true);
	}

	//디텍팅 유닛을 만들어야 되는 상황
	// if they have cloaked units get a new goal asap
	if (!_enemyCloakedDetected && InformationManager::Instance().hasCloakedUnits)
	{
		std::cout << "_enemyCloakedDetected" << std::endl;
		if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Missile_Turret) < 2)
		{
			buildQueue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Terran_Missile_Turret), BuildOrderItem::SeedPositionStrategy::FirstChokePoint, true);
			if (InformationManager::Instance().getFirstExpansionLocation(BWAPI::Broodwar->self()) != nullptr){
				buildQueue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Terran_Missile_Turret), BuildOrderItem::SeedPositionStrategy::FirstExpansionLocation, true);
			}
		}

		if (BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) == 0)
		{
			buildQueue.queueAsHighestPriority(MetaType(BWAPI::UnitTypes::Terran_Engineering_Bay), true);
		}

		if (Config::Debug::DrawBuildOrderSearchInfo)
		{
			BWAPI::Broodwar->printf("Enemy Cloaked Unit Detected!");
		}

		_enemyCloakedDetected = true;
	}

	checkBuildOrderQueueDeadlockAndRemove();

	//커맨드센터는 작업이 없으면 일꾼을 만든다.
	executeWorkerTraining();
}

void BuildManager::consumeBuildQueue(){
	// the current item to be used
	if (buildQueue.isEmpty()) return;
		
	BuildOrderItem currentItem = buildQueue.getHighestPriorityItem();

	// while there is still something left in the buildQueue
	while (!buildQueue.isEmpty())
	{
		bool isOkToRemoveQueue = true;

		// this is the unit which can produce the currentItem
		BWAPI::Unit producer = getProducer(currentItem.metaType, BWAPI::Position(currentItem.seedLocation), currentItem.producerID);

		bool canMake;

		// 건물을 만들수 있는 유닛(일꾼)이나, 유닛을 만들수 있는 유닛(건물 or 유닛)이 있으면
		if (producer != nullptr) {

			// check to see if we can make it right now
			// 지금 해당 유닛을 건설/생산 할 수 있는지에 대해 자원, 서플라이, 테크 트리, producer 만을 갖고 판단한다
			canMake = canMakeNow(producer, currentItem.metaType);
		}

		// if we can make the current item, create it
		if (producer != nullptr && canMake == true)
		{
			MetaType t = currentItem.metaType;

			if (t.isUnit())
			{
				if (t.getUnitType().isBuilding()) {
					// 테란 Addon 건물의 경우 (Addon 건물을 지을수 있는지는 getProducer 함수에서 이미 체크완료)
					// 모건물이 Addon 건물 짓기 전에는 canBuildAddon = true, isConstructing = false, canCommand = true 이다가 
					// Addon 건물을 짓기 시작하면 canBuildAddon = false, isConstructing = true, canCommand = true 가 되고 (Addon 건물 건설 취소는 가능하나 Train 등 커맨드는 불가능)
					// 완성되면 canBuildAddon = false, isConstructing = false 가 된다
					if (t.getUnitType().isAddon()) {
						producer->buildAddon(t.getUnitType());	

						//@도주남 애드온 못만들어도 그냥 지우기로(꼭 팩토리에 다 붙어 있을 필요는 없으므로)
						// 테란 Addon 건물의 경우 정상적으로 buildAddon 명령을 내려도 SCV가 모건물 근처에 있을 때 한동안 buildAddon 명령이 취소되는 경우가 있어서
						// 모건물이 isConstructing = true 상태로 바뀐 것을 확인한 후 buildQueue 에서 제거해야한다
						//if (producer->isConstructing() == false) {
						//	isOkToRemoveQueue = false;
						//}
					}
					// 그외 대부분 건물의 경우
					else
					{
						// ConstructionPlaceFinder 를 통해 건설 가능 위치 desiredPosition 를 알아내서
						// ConstructionManager 의 ConstructionTask Queue에 추가를 해서 desiredPosition 에 건설을 하게 한다. 
						// ConstructionManager 가 건설 도중에 해당 위치에 건설이 어려워지면 다시 ConstructionPlaceFinder 를 통해 건설 가능 위치를 desiredPosition 주위에서 찾을 것이다
						BWAPI::TilePosition desiredPosition = getDesiredPosition(t.getUnitType(), currentItem.seedLocation, currentItem.seedLocationStrategy);

						if (desiredPosition != BWAPI::TilePositions::None) {
							ConstructionManager::Instance().addConstructionTask(t.getUnitType(), desiredPosition);
						}
						else {
							// 건물 가능 위치가 없는 경우는, Protoss_Pylon 가 없거나, Creep 이 없거나, Refinery 가 이미 다 지어져있거나, 정말 지을 공간이 주위에 없는 경우인데,
							// 대부분의 경우 Pylon 이나 Hatchery가 지어지고 있는 중이므로, 다음 frame 에 건물 지을 공간을 다시 탐색하도록 한다. 
							std::cout << "There is no place to construct " << currentItem.metaType.getUnitType().getName().c_str()
								<< " strategy " << currentItem.seedLocationStrategy
								<< " seedPosition " << currentItem.seedLocation.x << "," << currentItem.seedLocation.y
								<< " desiredPosition " << desiredPosition.x << "," << desiredPosition.y << std::endl;

							isOkToRemoveQueue = false;
						}
					}
				}
				// 지상유닛 / 공중유닛의 경우
				else {
					producer->train(t.getUnitType());
				}
			}
			// if we're dealing with a tech research
			else if (t.isTech())
			{
				producer->research(t.getTechType());
			}
			else if (t.isUpgrade())
			{
				producer->upgrade(t.getUpgradeType());
			}

			// remove it from the buildQueue
			if (isOkToRemoveQueue) {
				buildQueue.removeCurrentItem();
			}

			// don't actually loop around in here
			// 한프레임에 빌드 한개 처리
			break;
		}

		// otherwise, if we can skip the current item
		// 그 빌드가 producer가 없거나 canmake가 안되면 스킵을 할수가 있음, 하지만 대부분 이미 BOSS를 통해 가능한 빌드가 들어온것이므로 미네랄 모일때까지 기다려야 하므로 블록킹 빌드로 쌓아야된다.
		// canSkipCurrentItem 에서 현재 빌드가 blocking 인지보고 스킵여부 결정함
		// 스킵해도 가져올 빌드가 없으면 스킵 불가
		// 현재 빌드가 블록킹 빌드이면 다음으로 진행 안함
		else if (buildQueue.canSkipCurrentItem())
		{
			// skip it and get the next one
			buildQueue.skipCurrentItem();
			currentItem = buildQueue.getNextItem();
		}
		else
		{
			//자원이 충분한데 처리가 안되면 일단 블로킹을 해제해서 자원수급을 원활하게 돌림
			if (currentItem.blocking && hasEnoughResources(currentItem.metaType)){
				buildQueue.setCurrentItemBlocking(false);
				//std::cout << currentItem.metaType.getName() << "is changed non block item" << std::endl;
			}

			break;
		}
	}
}

void BuildManager::performBuildOrderSearch()
{
	/*
	if (!Config::Modules::UsingBuildOrderSearch || !canPlanBuildOrderNow())
	{
		return;
	}
	*/

	BuildOrder & buildOrder = BOSSManager::Instance().getBuildOrder();

	if (buildOrder.size() > 0)
	{
		setBuildOrder(buildOrder);
		BOSSManager::Instance().reset();
	}
	else
	{
		if (!BOSSManager::Instance().isSearchInProgress())
		{
			const std::vector<MetaPair> & goalUnits = StrategyManager::Instance().getBuildOrderGoal();
			if (goalUnits.empty()){
				return;
			}

			BOSSManager::Instance().startNewSearch(goalUnits);
		}
	}
}

BWAPI::Unit BuildManager::getProducer(MetaType t, BWAPI::Position closestTo, int producerID)
{
	// get the type of unit that builds this
	//지을수 있는 타입을 고른다. 시즈면 팩토리
	BWAPI::UnitType producerType = t.whatBuilds();

	// make a set of all candidate producers
	//위에서 찾은 타입(팩토리)을 내 유닛중에 있는지 본다. 
	BWAPI::Unitset candidateProducers;
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit == nullptr) continue;

		// reasons a unit can not train the desired type
		if (unit->getType() != producerType)                    { continue; }
		if (!unit->exists())	                                { continue; }
		if (!unit->isCompleted())                               { continue; }
		if (unit->isTraining())                                 { continue; }
		if (unit->isResearching())                              { continue; }
		if (unit->isUpgrading())                                { continue; }
		// if unit is lifted, unit should land first
		if (unit->isLifted())                                   { continue; }

		//생산자를 선택한 경우, 그 생산자로 지정
		if (producerID != -1 && unit->getID() != producerID)	{ continue; }

		// if the type is an addon 
		// 애드온을 지으려면, 생산자에 이미 달려있는지, 애드온 짓는 지역이 비어 있는지 추가적으로 조사해야 됨
		if (t.getUnitType().isAddon())
		{
			/* 아주 예외사항
			1. (검증필요) 모건물은 건설되고 있는 중에는 isCompleted = false, isConstructing = true, canBuildAddon = false 이다가 건설이 완성된 후 몇 프레임동안은 isCompleted = true 이지만, canBuildAddon = false 인 경우가 있다
			2. if we just told this unit to build an addon, then it will not be building another one
			this deals with the frame-delay of telling a unit to build an addon and it actually starting to build
			빌드온 짓는 명령한지 10프레임이 안됐다면 제외, 딴 명령으로 바뀔수도 있나보다.
			*/
			//1. (검증필요) if (!unit->canBuildAddon()) { continue; }
			//2. 프레임 딜레이 및 애드온 가지고 있는지 판단
			if (hasAddon(unit)) continue;

			bool isBlocked = false;

			// if the unit doesn't have space to build an addon, it can't make one
			BWAPI::TilePosition addonPosition(
				unit->getTilePosition().x + unit->getType().tileWidth(),
				unit->getTilePosition().y + unit->getType().tileHeight() - t.getUnitType().tileHeight());

			for (int i = 0; i < t.getUnitType().tileWidth(); ++i)
			{
				for (int j = 0; j < t.getUnitType().tileHeight(); ++j)
				{
					BWAPI::TilePosition tilePos(addonPosition.x + i, addonPosition.y + j);

					// if the map won't let you build here, we can't build it.  
					// 맵 타일 자체가 건설 불가능한 타일인 경우 + 기존 건물이 해당 타일에 이미 있는경우
					if (!BWAPI::Broodwar->isBuildable(tilePos, true))
					{
						isBlocked = true;
					}

					// if there are any units on the addon tile, we can't build it
					// 아군 유닛은 Addon 지을 위치에 있어도 괜찮음. (적군 유닛은 Addon 지을 위치에 있으면 건설 안되는지는 아직 불확실함)
					BWAPI::Unitset uot = BWAPI::Broodwar->getUnitsOnTile(tilePos.x, tilePos.y);
					for (auto & u : uot) {
						if (u->getPlayer() != InformationManager::Instance().selfPlayer) {
							isBlocked = true;
							break;
						}
					}
				}
			}

			if (isBlocked)
			{
				continue;
			}
		}



		// if the type requires an addon and the producer doesn't have one
		//짓는 건물 외에 필요정보들을 가지고 와서 필요한 애드온 및 건물 확인
		//고스트 -> whatbuild는 배럭, requiredUnits은 아카데미와 코벌트옵스
		//타겟유닛(시즈)을 만드는데 필요한 추가 정보를 읽어와서 필요한 것이 애드온인 경우 애드온이 있는지 확인.
		//생산자와 애드온이 연결되어 잇어야 되는 경우와, 그냥 있기만 하면 되는 경우로 나뉨
		bool except_candidates = false;
		typedef std::pair<BWAPI::UnitType, int> ReqPair;
		for (const ReqPair & pair : t.getUnitType().requiredUnits())
		{
			BWAPI::UnitType requiredType = pair.first;
			//필요타입이 애드온 이면서 생산자와 연결되어 있어야 되는 경우(탱크, 드랍쉽, 사배 등)
			if (requiredType.isAddon() && requiredType.whatBuilds().first == unit->getType()){
				//생산자에 에드온이 없는 경우
				//생산자에 달린 애드온이 다른 애드온인 경우(테란에는 애드온이 2개씩 있는 건물이 있음)
				if (!unit->getAddon() || unit->getAddon() == nullptr || unit->getAddon()->getType() != requiredType){
					except_candidates = true;
					break;
				}
			}
		}

		if (except_candidates) continue;

        // if we haven't cut it, add it to the set of candidates
        candidateProducers.insert(unit);
    }

	/*
	if (t.getUnitType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || t.getUnitType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || t.getUnitType() == BWAPI::UnitTypes::Terran_Machine_Shop){
	std::cout << t.getUnitType() << " candidateProducers:";
	for (auto &u : candidateProducers){
	std::cout << u->getType() << "(" << u->getPosition() << ")";
	}
	std::cout << std::endl;
	}
	*/


	return getClosestUnitToPosition(candidateProducers, closestTo);;
}

BWAPI::Unit BuildManager::getClosestUnitToPosition(const BWAPI::Unitset & units, BWAPI::Position closestTo)
{
    if (units.size() == 0)
    {
        return nullptr;
    }

    // if we don't care where the unit is return the first one we have
	if (closestTo == BWAPI::Positions::None)
    {
        return *(units.begin());
    }

    BWAPI::Unit closestUnit = nullptr;
    double minDist(1000000000);

	for (auto & unit : units) 
    {
		if (unit == nullptr) continue;

		double distance = unit->getDistance(closestTo);
		if (!closestUnit || distance < minDist) 
        {
			closestUnit = unit;
			minDist = distance;
		}
	}

    return closestUnit;
}

// 지금 해당 유닛을 건설/생산 할 수 있는지에 대해 자원, 서플라이, 테크 트리, producer 만을 갖고 판단한다
// 해당 유닛이 건물일 경우 건물 지을 위치의 적절 여부 (탐색했었던 타일인지, 건설 가능한 타일인지, 주위에 Pylon이 있는지, Creep이 있는 곳인지 등) 는 판단하지 않는다
bool BuildManager::canMakeNow(BWAPI::Unit producer, MetaType t)
{
	if (producer == nullptr) {
		return false;
	}

	bool canMake = hasEnoughResources(t);

	if (canMake)
	{
		if (t.isUnit())
		{
			// BWAPI::Broodwar->canMake : Checks all the requirements include resources, supply, technology tree, availability, and required units
			canMake = BWAPI::Broodwar->canMake(t.getUnitType(), producer); //컴셋 지으려고 하는데 아카데미 없는 경우, producer는 SCV로 지정되었다고 해도 false
		}
		else if (t.isTech())
		{
			canMake = BWAPI::Broodwar->canResearch(t.getTechType(), producer);
		}
		else if (t.isUpgrade())
		{
			canMake = BWAPI::Broodwar->canUpgrade(t.getUpgradeType(), producer);
		}
	}

	return canMake;
}

// 건설 가능 위치를 찾는다
// seedLocationStrategy 가 SeedPositionSpecified 인 경우에는 그 근처만 찾아보고, SeedPositionSpecified 이 아닌 경우에는 seedLocationStrategy 를 조금씩 바꿔가며 계속 찾아본다.
// (MainBase -> MainBase 주위 -> MainBase 길목 -> MainBase 가까운 앞마당 -> MainBase 가까운 앞마당의 길목 -> 다른 멀티 위치 -> 탐색 종료)
BWAPI::TilePosition BuildManager::getDesiredPosition(BWAPI::UnitType unitType, BWAPI::TilePosition seedPosition, BuildOrderItem::SeedPositionStrategy seedPositionStrategy)
{
	BWAPI::TilePosition desiredPosition = ConstructionPlaceFinder::Instance().getBuildLocationWithSeedPositionAndStrategy(unitType, seedPosition, seedPositionStrategy);

	/*
	 std::cout << "ConstructionPlaceFinder getBuildLocationWithSeedPositionAndStrategy "
		<< unitType.getName().c_str()
		<< " strategy " << seedPositionStrategy
		<< " seedPosition " << seedPosition.x << "," << seedPosition.y
		<< " desiredPosition " << desiredPosition.x << "," << desiredPosition.y << std::endl;
	*/

	// desiredPosition 을 찾을 수 없는 경우
	bool findAnotherPlace = true;
	while (desiredPosition == BWAPI::TilePositions::None) {

		switch (seedPositionStrategy) {
		case BuildOrderItem::SeedPositionStrategy::MainBaseLocation:
			seedPositionStrategy = BuildOrderItem::SeedPositionStrategy::MainBaseBackYard;
			break;
		case BuildOrderItem::SeedPositionStrategy::MainBaseBackYard:
			seedPositionStrategy = BuildOrderItem::SeedPositionStrategy::FirstChokePoint;
			break;
		case BuildOrderItem::SeedPositionStrategy::FirstChokePoint:
			seedPositionStrategy = BuildOrderItem::SeedPositionStrategy::FirstExpansionLocation;
			break;
		case BuildOrderItem::SeedPositionStrategy::FirstExpansionLocation:
			seedPositionStrategy = BuildOrderItem::SeedPositionStrategy::SecondChokePoint;
			break;
		case BuildOrderItem::SeedPositionStrategy::SecondChokePoint:
			seedPositionStrategy = BuildOrderItem::SeedPositionStrategy::SecondExpansionLocation;
			break;
		case BuildOrderItem::SeedPositionStrategy::SecondExpansionLocation:
		case BuildOrderItem::SeedPositionStrategy::SeedPositionSpecified:
		default:
			findAnotherPlace = false;
			break;
		}

		// 다른 곳을 더 찾아본다
		if (findAnotherPlace) {
			desiredPosition = ConstructionPlaceFinder::Instance().getBuildLocationWithSeedPositionAndStrategy(unitType, seedPosition, seedPositionStrategy);
			/*
			 std::cout << "ConstructionPlaceFinder getBuildLocationWithSeedPositionAndStrategy "
				<< unitType.getName().c_str()
				<< " strategy " << seedPositionStrategy
				<< " seedPosition " << seedPosition.x << "," << seedPosition.y
				<< " desiredPosition " << desiredPosition.x << "," << desiredPosition.y << std::endl;
			*/
		}
		// 다른 곳을 더 찾아보지 않고, 끝낸다
		else {
			break;
		}
	}

	return desiredPosition;
}

// 사용가능 미네랄 = 현재 보유 미네랄 - 사용하기로 예약되어있는 미네랄
int BuildManager::getAvailableMinerals()
{
	return BWAPI::Broodwar->self()->minerals() - ConstructionManager::Instance().getReservedMinerals();
}

// 사용가능 가스 = 현재 보유 가스 - 사용하기로 예약되어있는 가스
int BuildManager::getAvailableGas()
{
	return BWAPI::Broodwar->self()->gas() - ConstructionManager::Instance().getReservedGas();
}

// return whether or not we meet resources, including building reserves
bool BuildManager::hasEnoughResources(MetaType type) 
{
	// return whether or not we meet the resources
	return (type.mineralPrice() <= getAvailableMinerals()) && (type.gasPrice() <= getAvailableGas());
}


// selects a unit of a given type
BWAPI::Unit BuildManager::selectUnitOfType(BWAPI::UnitType type, BWAPI::Position closestTo) 
{
	// if we have none of the unit type, return nullptr right away
	if (BWAPI::Broodwar->self()->completedUnitCount(type) == 0) 
	{
		return nullptr;
	}

	BWAPI::Unit unit = nullptr;

	// if we are concerned about the position of the unit, that takes priority
    if (closestTo != BWAPI::Positions::None) 
    {
		double minDist(1000000000);

		for (auto & u : BWAPI::Broodwar->self()->getUnits()) 
        {
			if (u->getType() == type) 
            {
				double distance = u->getDistance(closestTo);
				if (!unit || distance < minDist) {
					unit = u;
					minDist = distance;
				}
			}
		}

	// if it is a building and we are worried about selecting the unit with the least
	// amount of training time remaining
	} 
    else if (type.isBuilding()) 
    {
		for (auto & u : BWAPI::Broodwar->self()->getUnits()) 
        {
			if (u->getType() == type && u->isCompleted() && !u->isTraining() && !u->isLifted() &&u->isPowered()) {

				return u;
			}
		}
		// otherwise just return the first unit we come across
	} 
    else 
    {
		for (auto & u : BWAPI::Broodwar->self()->getUnits()) 
		{
			if (u->getType() == type && u->isCompleted() && u->getHitPoints() > 0 && !u->isLifted() &&u->isPowered()) 
			{
				return u;
			}
		}
	}

	// return what we've found so far
	return nullptr;
}


BuildManager & BuildManager::Instance()
{
	static BuildManager instance;
	return instance;
}

BuildOrderQueue * BuildManager::getBuildQueue()
{
	return & buildQueue;
}

bool BuildManager::isProducerWillExist(BWAPI::UnitType producerType)
{
	bool isProducerWillExist = true;

	if (BWAPI::Broodwar->self()->completedUnitCount(producerType) == 0
		&& BWAPI::Broodwar->self()->incompleteUnitCount(producerType) == 0)
	{
		// producer 가 건물 인 경우 : 건물이 건설 중인지 추가 파악
		if (producerType.isBuilding()) {
			if (ConstructionManager::Instance().getConstructionQueueItemCount(producerType) == 0) {
				
				// producerType이 Addon 건물인 경우, Addon 건물 건설이 명령 내려졌지만 시작되기 직전에는 getUnits, completedUnitCount, incompleteUnitCount 에서 확인할 수 없다
				// producerType의 producerType 건물에 의해 Addon 건물 건설의 명령이 들어갔는지까지 확인해야 한다
				if (producerType.isAddon()) {
					bool isAddonConstructing = false;
					BWAPI::UnitType producerTypeOfProducerType = producerType.whatBuilds().first;
					for (auto & unit : BWAPI::Broodwar->self()->getUnits())
					{
						if (unit->getType() == producerTypeOfProducerType){
							// 모건물이 완성되어있고, 모건물이 해당 Addon 건물을 건설중인지 확인한다
							if (unit->isCompleted() && unit->isConstructing() && unit->getBuildType() == producerType) {
								isAddonConstructing = true;
								break;
							}
						}
					}

					//필요한 애드온 안지어 지고 있음
					if (isAddonConstructing == false) {
						isProducerWillExist = false;
					}
				}
				else {
					isProducerWillExist = false;
				}
			}
		}
		// producer 가 건물이 아닌 경우 : producer 가 생성될 예정인지 추가 파악
		// producerType : 일꾼
		else {
			isProducerWillExist = false;
		}
	}

	return isProducerWillExist;
}

void BuildManager::checkBuildOrderQueueDeadlockAndAndFixIt()
{
	// BuildQueue 의 HighestPriority 에 있는 BuildQueueItem 이 skip 불가능한 것인데, 선행조건이 충족될 수 없거나, 실행이 앞으로도 계속 불가능한 경우, dead lock 이 발생한다
	// 선행 건물을 BuildQueue에 추가해넣을지, 해당 BuildQueueItem 을 삭제할지 전략적으로 판단해야 한다
	BuildOrderQueue * buildQueue = BuildManager::Instance().getBuildQueue();
	if (!buildQueue->isEmpty())
	{
		BuildOrderItem currentItem = buildQueue->getHighestPriorityItem();

		// TODO : 수정. canSkipCurrentItem 문제있는가?
		//if (buildQueue->canSkipCurrentItem() == false)
		if (currentItem.blocking == true)
		{
			bool isDeadlockCase = false;

			// producerType을 먼저 알아낸다
			BWAPI::UnitType producerType = currentItem.metaType.whatBuilds();

			// 건물이나 유닛의 경우
			if (currentItem.metaType.isUnit())
			{
				BWAPI::UnitType unitType = currentItem.metaType.getUnitType();
				BWAPI::TechType requiredTechType = unitType.requiredTech();
				const std::map< BWAPI::UnitType, int >& requiredUnits = unitType.requiredUnits();
				int requiredSupply = unitType.supplyRequired();

				// 건물을 생산하는 유닛이나, 유닛을 생산하는 건물이 존재하지 않고, 건설 예정이지도 않으면 dead lock
				if (isProducerWillExist(producerType) == false) {
					isDeadlockCase = true;
				}

				// Refinery 건물의 경우, Refinery 가 건설되지 않은 Geyser가 있는 경우에만 가능
				if (!isDeadlockCase && unitType == InformationManager::Instance().getRefineryBuildingType())
				{
					bool hasAvailableGeyser = true;

					// Refinery가 지어질 수 있는 장소를 찾아본다
					BWAPI::TilePosition testLocation = getDesiredPosition(unitType, currentItem.seedLocation, currentItem.seedLocationStrategy);
					
					// Refinery 를 지으려는 장소를 찾을 수 없으면 dead lock
					if (testLocation == BWAPI::TilePositions::None || testLocation == BWAPI::TilePositions::Invalid || testLocation.isValid() == false) {
						std::cout << "Build Order Dead lock case -> Cann't find place to construct " << unitType.getName() << std::endl;
						hasAvailableGeyser = false;
					}
					else {
						// Refinery 를 지으려는 장소에 Refinery 가 이미 건설되어 있다면 dead lock 
						BWAPI::Unitset uot = BWAPI::Broodwar->getUnitsOnTile(testLocation);
						for (auto & u : uot) {
							if (u->getType().isRefinery() && u->exists()) {
								hasAvailableGeyser = false;
								break;
							}
						}
					}

					if (hasAvailableGeyser == false) {
						isDeadlockCase = true;
					}
				}
				
				// 선행 기술 리서치가 되어있지 않고, 리서치 중이지도 않으면 dead lock
				if (!isDeadlockCase && requiredTechType != BWAPI::TechTypes::None)
				{
					if (BWAPI::Broodwar->self()->hasResearched(requiredTechType) == false) {
						if (BWAPI::Broodwar->self()->isResearching(requiredTechType) == false) {
							isDeadlockCase = true;
						}
					}
				}

				// 선행 건물/유닛이 있는데 
				if (!isDeadlockCase && requiredUnits.size() > 0)
				{
					for (auto & u : requiredUnits)
					{
						BWAPI::UnitType requiredUnitType = u.first;

						if (requiredUnitType != BWAPI::UnitTypes::None) {

							/*
							std::cout << "pre requiredUnitType " << requiredUnitType.getName()
							<< " completedUnitCount " << BWAPI::Broodwar->self()->completedUnitCount(requiredUnitType)
							<< " incompleteUnitCount " << BWAPI::Broodwar->self()->incompleteUnitCount(requiredUnitType)
							<< std::endl;
							*/

							// 선행 건물 / 유닛이 존재하지 않고, 생산 중이지도 않고
							if (BWAPI::Broodwar->self()->completedUnitCount(requiredUnitType) == 0
								&& BWAPI::Broodwar->self()->incompleteUnitCount(requiredUnitType) == 0)
							{
								// 선행 건물이 건설 예정이지도 않으면 dead lock
								if (requiredUnitType.isBuilding())
								{
									if (ConstructionManager::Instance().getConstructionQueueItemCount(requiredUnitType) == 0) {
										isDeadlockCase = true;
									}
								}
								// 선행 유닛이 Larva 인 Zerg 유닛의 경우, Larva, Hatchery, Lair, Hive 가 하나도 존재하지 않고, 건설 예정이지 않은 경우에 dead lock
								else if (requiredUnitType == BWAPI::UnitTypes::Zerg_Larva)
								{
									if (BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Zerg_Hatchery) == 0
										&& BWAPI::Broodwar->self()->incompleteUnitCount(BWAPI::UnitTypes::Zerg_Hatchery) == 0
										&& BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Zerg_Lair) == 0
										&& BWAPI::Broodwar->self()->incompleteUnitCount(BWAPI::UnitTypes::Zerg_Lair) == 0
										&& BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Zerg_Hive) == 0
										&& BWAPI::Broodwar->self()->incompleteUnitCount(BWAPI::UnitTypes::Zerg_Hive) == 0)
									{
										if (ConstructionManager::Instance().getConstructionQueueItemCount(BWAPI::UnitTypes::Zerg_Hatchery) == 0
											&& ConstructionManager::Instance().getConstructionQueueItemCount(BWAPI::UnitTypes::Zerg_Lair) == 0
											&& ConstructionManager::Instance().getConstructionQueueItemCount(BWAPI::UnitTypes::Zerg_Hive) == 0)
										{
											isDeadlockCase = true;
										}
									}
								}
							}
						}
					}
				}

				// 건물이 아닌 지상/공중 유닛인 경우, 서플라이가 400 꽉 찼으면 dead lock
				if (!isDeadlockCase && !unitType.isBuilding()
					&& BWAPI::Broodwar->self()->supplyTotal() == 400 && BWAPI::Broodwar->self()->supplyUsed() + unitType.supplyRequired() > 400)
				{
					isDeadlockCase = true;
				}

				// 건물이 아닌 지상/공중 유닛인 경우, 서플라이가 부족하면 dead lock 이지만, 서플라이 부족하다고 지상/공중유닛 빌드를 취소하기보다는 빨리 서플라이를 짓도록 하기 위해, 
				// 이것은 StrategyManager 등에서 따로 처리하도록 한다 

				//@도주남 
				/*
				if (!isDeadlockCase && unitType == BWAPI::UnitTypes::Terran_Barracks){
					int _max_barracks = 4;
					if ((int(UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Barracks)) + ConstructionManager::Instance().getConstructionQueueItemCount(unitType)) >= _max_barracks){
						isDeadlockCase = true;
					}
				}
				*/

			}
			// 테크의 경우, 해당 리서치를 이미 했거나, 이미 하고있거나, 리서치를 하는 건물 및 선행건물이 존재하지않고 건설예정이지도 않으면 dead lock
			else if (currentItem.metaType.isTech())
			{
				BWAPI::TechType techType = currentItem.metaType.getTechType();
				BWAPI::UnitType requiredUnitType = techType.requiredUnit();

				if (BWAPI::Broodwar->self()->hasResearched(techType) || BWAPI::Broodwar->self()->isResearching(techType)) {
					isDeadlockCase = true;
				}
				else if (BWAPI::Broodwar->self()->completedUnitCount(producerType) == 0
					&& BWAPI::Broodwar->self()->incompleteUnitCount(producerType) == 0)
				{
					if (ConstructionManager::Instance().getConstructionQueueItemCount(producerType) == 0) {

						// 테크 리서치의 producerType이 Addon 건물인 경우, Addon 건물 건설이 명령 내려졌지만 시작되기 직전에는 getUnits, completedUnitCount, incompleteUnitCount 에서 확인할 수 없다
						// producerType의 producerType 건물에 의해 Addon 건물 건설의 명령이 들어갔는지까지 확인해야 한다
						if (producerType.isAddon()) {

							bool isAddonConstructing = false;

							BWAPI::UnitType producerTypeOfProducerType = producerType.whatBuilds().first;

							if (producerTypeOfProducerType != BWAPI::UnitTypes::None) {

								for (auto & unit : BWAPI::Broodwar->self()->getUnits())
								{
									if (unit == nullptr) continue;
									if (unit->getType() != producerTypeOfProducerType)	{ continue; }

									// 모건물이 완성되어있고, 모건물이 해당 Addon 건물을 건설중인지 확인한다
									if (unit->isCompleted() && unit->isConstructing() && unit->getBuildType() == producerType) {
										isAddonConstructing = true;
										break;
									}
								}
							}

							if (isAddonConstructing == false) {
								isDeadlockCase = true;
							}
						}
						else {
							isDeadlockCase = true;
						}
					}
				}
				else if (requiredUnitType != BWAPI::UnitTypes::None) {
					if (BWAPI::Broodwar->self()->completedUnitCount(requiredUnitType) == 0
						&& BWAPI::Broodwar->self()->incompleteUnitCount(requiredUnitType) == 0) {
						if (ConstructionManager::Instance().getConstructionQueueItemCount(requiredUnitType) == 0) {
							isDeadlockCase = true;
						}
					}
				}
			}
			// 업그레이드의 경우, 해당 업그레이드를 이미 했거나, 이미 하고있거나, 업그레이드를 하는 건물 및 선행건물이 존재하지도 않고 건설예정이지도 않으면 dead lock
			else if (currentItem.metaType.isUpgrade())
			{
				BWAPI::UpgradeType upgradeType = currentItem.metaType.getUpgradeType();
				int maxLevel = BWAPI::Broodwar->self()->getMaxUpgradeLevel(upgradeType);
				int currentLevel = BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType);
				BWAPI::UnitType requiredUnitType = upgradeType.whatsRequired();

				if (currentLevel >= maxLevel || BWAPI::Broodwar->self()->isUpgrading(upgradeType)) {
					isDeadlockCase = true;
				}
				else if (BWAPI::Broodwar->self()->completedUnitCount(producerType) == 0
					&& BWAPI::Broodwar->self()->incompleteUnitCount(producerType) == 0) {
					if (ConstructionManager::Instance().getConstructionQueueItemCount(producerType) == 0) {

						// 업그레이드의 producerType이 Addon 건물인 경우, Addon 건물 건설이 시작되기 직전에는 getUnits, completedUnitCount, incompleteUnitCount 에서 확인할 수 없다
						// producerType의 producerType 건물에 의해 Addon 건물 건설이 시작되었는지까지 확인해야 한다						
						if (producerType.isAddon()) {

							bool isAddonConstructing = false;

							BWAPI::UnitType producerTypeOfProducerType = producerType.whatBuilds().first;

							if (producerTypeOfProducerType != BWAPI::UnitTypes::None) {

								for (auto & unit : BWAPI::Broodwar->self()->getUnits())
								{
									if (unit == nullptr) continue;
									if (unit->getType() != producerTypeOfProducerType)	{ continue; }
									// 모건물이 완성되어있고, 모건물이 해당 Addon 건물을 건설중인지 확인한다
									if (unit->isCompleted() && unit->isConstructing() && unit->getBuildType() == producerType) {
										isAddonConstructing = true;
										break;
									}
								}
							}

							if (isAddonConstructing == false) {
								isDeadlockCase = true;
							}
						}
						else {
							isDeadlockCase = true;
						}
					}
				}
				else if (requiredUnitType != BWAPI::UnitTypes::None) {
					if (BWAPI::Broodwar->self()->completedUnitCount(requiredUnitType) == 0
						&& BWAPI::Broodwar->self()->incompleteUnitCount(requiredUnitType) == 0) {
						if (ConstructionManager::Instance().getConstructionQueueItemCount(requiredUnitType) == 0) {
							isDeadlockCase = true;
						}
					}
				}
			}

			if (isDeadlockCase) {
				std::cout << std::endl << "Build Order Dead lock case -> remove BuildOrderItem " << currentItem.metaType.getName() << std::endl;

				buildQueue->removeCurrentItem();
			}

		}
	}

}


void BuildManager::checkBuildOrderQueueDeadlockAndRemove()
{
	/*
		getProducer에서 대부분 로직 체크. 다만, 기다려야 되는 상황이 있기 때문에 당장 처리하지 못하면 non block빌드로 전환하여 다음 기회에 수행할 수 있도록 한다.
		모두 non block 빌드가 된 경우에는 빌드큐에 있는 다른 빌드가 수행된 이후에도 이 빌드를 처리할 수 없는 상태이므로(즉, 순서 잘못이 아니다. 아에 빌드큐에 있는 것으로 해결이 안되는 상황)
		이 때는 필요한 건물이 현재 건설중인 건물이 있지 않으면 삭제처리한다.
	*/
	//큐를 lowest부터 돌면서 non block 빌드만 남았는지 체크 -> 모두다 non block이면 뭔가 문제들이 있었던 빌드이므로 아래 로직 체크해서 하나씩 지운다.
	for (int i = 0; i <buildQueue.size(); i++){
		if (buildQueue[i].blocking) return;
	}

	/*
	std::cout << "every build is non block" << std::endl;
	for (int i = 0; i <buildQueue.size() - 1; i++){
		std::cout << buildQueue[i].metaType.getName() << " ";
	}
	std::cout << std::endl;
	*/

	for (int i = buildQueue.size()-1; i >=0 ; i--){
		bool isDeadlockCase = false;
		BuildOrderItem &currentItem = buildQueue[i];

		// producerType을 먼저 알아낸다
		BWAPI::UnitType producerType = currentItem.metaType.whatBuilds();

		//std::cout << "target:" << currentItem.metaType.getName() << "/producer:" << producerType;

		// 건물이나 유닛의 경우
		if (currentItem.metaType.isUnit())
		{
			BWAPI::UnitType unitType = currentItem.metaType.getUnitType();
			const std::map< BWAPI::UnitType, int >& requiredUnits = unitType.requiredUnits();

			// 건물을 생산하는 유닛이나, 유닛을 생산하는 건물이 존재하지 않고, 건설 예정이지도 않으면 dead lock
			if (!isProducerWillExist(producerType)) {
				isDeadlockCase = true;
			}
			// 애드온인 경우, 모든 팩토리가 애드온이 달렸거나 달리는중이면 데드락
			else if (currentItem.metaType.getUnitType().isAddon()){
				bool all_producer_has_addon = true;
				for (BWAPI::Unit u : BWAPI::Broodwar->self()->getUnits()){
					if (u->getType() == producerType && !hasAddon(u)){
						all_producer_has_addon = false;
						break;
					}
				}

				if (all_producer_has_addon) isDeadlockCase = true;
			}

			// Refinery 건물의 경우, Refinery 가 건설되지 않은 Geyser가 있는 경우에만 가능
			// TODO TODO TODO TODO

			// 선행 건물/애드온이 있는데 
			if (!isDeadlockCase)
			{
				for (auto & u : requiredUnits)
				{
					// 선행 건물/애드온이 존재하지 않고, 건설 예정이지도 않으면 dead lock
					if (!isProducerWillExist(u.first)) {
						isDeadlockCase = true;
					}
				}
			}
		}


		// 테크의 경우, 해당 리서치를 이미 했거나, 이미 하고있거나, 리서치를 하는 건물 및 선행건물이 존재하지않고 건설예정이지도 않으면 dead lock
		else if (currentItem.metaType.isTech())
		{
			BWAPI::TechType techType = currentItem.metaType.getTechType();
			BWAPI::UnitType requiredUnitType = techType.requiredUnit(); //실제 테크에서 다른유닛을 필요로 하는 것은 러커변이만 레어를 필요로하고 나머지는 다 None

			if (BWAPI::Broodwar->self()->hasResearched(techType) || BWAPI::Broodwar->self()->isResearching(techType)) {
				isDeadlockCase = true;
			}
			else if (!isProducerWillExist(producerType))
			{
				isDeadlockCase = true;
			}
			else if (requiredUnitType != BWAPI::UnitTypes::None) {
				if (!isProducerWillExist(requiredUnitType))
					isDeadlockCase = true;
			}
		}
		// 업그레이드의 경우, 해당 업그레이드를 이미 했거나, 이미 하고있거나, 업그레이드를 하는 건물 및 선행건물이 존재하지도 않고 건설예정이지도 않으면 dead lock
		else if (currentItem.metaType.isUpgrade())
		{
			BWAPI::UpgradeType upgradeType = currentItem.metaType.getUpgradeType();
			int maxLevel = BWAPI::Broodwar->self()->getMaxUpgradeLevel(upgradeType);
			int currentLevel = BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType);
			BWAPI::UnitType requiredUnitType = upgradeType.whatsRequired(currentLevel);

			if (currentLevel >= maxLevel || BWAPI::Broodwar->self()->isUpgrading(upgradeType)) {
				isDeadlockCase = true;
			}
			else if (!isProducerWillExist(producerType))
			{
				isDeadlockCase = true;
			}
			else if (requiredUnitType != BWAPI::UnitTypes::None) {
				if (!isProducerWillExist(requiredUnitType))
					isDeadlockCase = true;
			}
		}

		if (isDeadlockCase) {
			std::cout << std::endl << "Build Order Dead lock case -> remove " << currentItem.metaType.getName() << std::endl;

			buildQueue.removeByIndex(i);
		}

	}
}

//BOSS에서 빌드가 나오면 세팅해주는 함수
//BOSS.getBuildOrder 해서 비어 있으면 그때까지는 계속 빌드큐가 비어있다.
//@도주남 김유진 설치전략 추가
void BuildManager::setBuildOrder(const BuildOrder & buildOrder)
{
	//buildQueue.clearAll(); //중간중간 빌드 추가되므로 삭제

	//현재 큐까지 값을 반영하여 limit 수치를 계산해야 하므로 카운트맵을 하나 만듬
	std::map<std::string, int> buildItemCnt;

	for (size_t i(0); i < buildOrder.size(); ++i){
		if (buildItemCnt.find(buildOrder[i].getName()) == buildItemCnt.end())
			buildItemCnt[buildOrder[i].getName()] = 0;
		else
			buildItemCnt[buildOrder[i].getName()]++;
	}

	for (size_t i(0); i<buildOrder.size(); ++i)
	{
		//유닛 최대값을 관리하여 그 이상은 안만들도록 한다.
		if (buildOrder[i].isUnit()){
			int unitLimit = StrategyManager::Instance().getUnitLimit(buildOrder[i]);
			int currentUnitNum = BWAPI::Broodwar->self()->completedUnitCount(buildOrder[i].getUnitType()) + BWAPI::Broodwar->self()->incompleteUnitCount(buildOrder[i].getUnitType()) + buildItemCnt[buildOrder[i].getName()];

			if (unitLimit != -1 && currentUnitNum > unitLimit){
				std::cout << "[unit limit]-" << buildOrder[i].getName() << "is only created " << unitLimit << std::endl;
				buildItemCnt[buildOrder[i].getName()]--;
				continue;
			}
		}

		if (buildOrder[i].isBuilding()){
			buildQueue.queueAsLowestPriority(buildOrder[i], StrategyManager::Instance().getBuildSeedPositionStrategy(buildOrder[i]), true);
		}
		else{
			buildQueue.queueAsLowestPriority(buildOrder[i], true);
		}
		
	}
}

void BuildManager::addBuildOrderOneItem(MetaType buildOrder, BWAPI::TilePosition position)
{
	buildQueue.queueAsHighestPriority(buildOrder, position, true);
}

//dhj ssh
void BuildManager::queueGasSteal()
{
	buildQueue.queueAsHighestPriority(MetaType(BWAPI::Broodwar->self()->getRace().getRefinery()), true, true);
}

void BuildManager::onUnitComplete(BWAPI::Unit unit){
	return;
}

bool BuildManager::detectSupplyDeadlock()
{
	// if the _queue is empty there is no deadlock
	if (buildQueue.size() == 0 || BWAPI::Broodwar->self()->supplyTotal() >= 390)
	{
		return false;
	}

	// are any supply providers being built currently
	// 건설큐에 있으면 건설할 것으로 판단
	// 건물은 isComplete 될때까지 건설큐에 있음
	bool supplyInProgress = false;
	if (ConstructionManager::Instance().getConstructionQueueItemCount(BWAPI::UnitTypes::Terran_Supply_Depot) > 0){
		supplyInProgress = true;
	}

	// does the current item being built require more supply

	int supplyCost = buildQueue.getHighestPriorityItem().metaType.supplyRequired();
	int supplyAvailable = std::max(0, BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed());

	// if we don't have enough supply and none is being built, there's a deadlock
	if ((supplyAvailable < supplyCost) && !supplyInProgress)
	{
		return true;
	}

	return false;
}

//아주 예외적임
//매프레임 빈 커맨드센터 1개씩 돌면서 일꾼생산
//단, BOSS매니저 계산중에는 동작하지 않음
//단, 초기빌드 수행시는 동작하지 않음
//멀티 공격받고 있으면 동작하지 않음
//해당 멀티 수급할 정도만 생산 
void BuildManager::executeWorkerTraining(){
	if (!StrategyManager::Instance().isInitialBuildOrderFinished){
		return;
	}

	if (BOSSManager::Instance().isSearchInProgress()){
		return;
	}
	
	//InformationManager::Instance().selfExpansions 사용안함
	//WorkerManager::Instance().getWorkerData().getDepots() 사용 %%%% 두개가 비슷한것으로 판단되나.. 각자 연관된 모듈이 다르므로 합치지 않고 간다.
	for (BWAPI::Unit u : ExpansionManager::Instance().getExpansions()){
		if (!u->isIdle()) continue;
		if (u->isUnderAttack()) continue;
		if (!verifyBuildAddonCommand(u)) continue; //빌드애드온 시작하자마자 다른 오더를 바로 내리면 안됨
		
		int tmpWorkerCnt = WorkerManager::Instance().getWorkerData().getDepotWorkerCount(u);

		//최대생산량 = 현재남은 미네랄덩어리수 * 1.5 * 초기가중치(1.3)
		if (tmpWorkerCnt > -1 && 
			tmpWorkerCnt < (int)(WorkerManager::Instance().getWorkerData().getMineralsNearDepot(u)*1.5*StrategyManager::Instance().weightByFrame(1.3)) ){
			u->train(BWAPI::UnitTypes::Terran_SCV);
			return;
		}
	}
}
bool BuildManager::verifyBuildAddonCommand(BWAPI::Unit u){
	//브루드워를 통과하는데 시간이 좀 걸림, 이 체크 안하고 isidle 같은걸로 확인이 안되서 명령이 덮어써짐
	if (u->getLastCommand().getType() == BWAPI::UnitCommandTypes::Build_Addon
		&& (BWAPI::Broodwar->getFrameCount() - u->getLastCommandFrame() < 10))
	{
		return false;
	}
	else{
		return true;
	}
}

bool BuildManager::hasAddon(BWAPI::Unit u){
	//프레임 딜레이 체크하여 딜레이 동안은 다른명령을 내리면 안됨(즉, 애드온이 있는것처럼 판단)
	if (!verifyBuildAddonCommand(u))
		return true;
	// if the unit already has an addon, it can't make one
	else if (u->getAddon() || u->getAddon() != nullptr)
		return true;
	else
		return false;
}

bool BuildManager::hasUnitInQueue(BWAPI::UnitType ut){
	for (int i = buildQueue.size() - 1; i >= 0; i--){
		BuildOrderItem &currentItem = buildQueue[i];
		if (currentItem.metaType.isUnit() &&
			currentItem.metaType.getUnitType() == ut)
			return true;
	}

	return false;
}