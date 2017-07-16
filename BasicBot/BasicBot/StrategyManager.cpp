#include "StrategyManager.h"
#include "CommandUtil.h"

using namespace MyBot;

StrategyManager & StrategyManager::Instance()
{
	static StrategyManager instance;
	return instance;
}

StrategyManager::StrategyManager()
{
	isFullScaleAttackStarted = false;
	isInitialBuildOrderFinished = false;
}

void StrategyManager::onStart()
{
	setOpeningBookBuildOrder();
}

void StrategyManager::setOpeningBookBuildOrder(){
	std::string str_build_order;
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran){
		//_main_strategy = "Terran_MarineRush";
		//_main_strategy = "Terran_Mechanic";
		_main_strategy = "Terran_Bionic";
		//str_build_order = "SCV SCV SCV SCV SCV Supply_Depot SCV Barracks SCV Barracks SCV SCV SCV Marine Supply_Depot SCV Marine Refinery SCV Marine SCV Marine SCV Marine Supply_Depot SCV SCV Academy SCV SCV Marine Supply_Depot Marine Marine Marine Marine Marine Marine Marine Stim_Packs Medic Medic Firebat Firebat";
		str_build_order = "SCV SCV SCV SCV SCV Supply_Depot SCV Barracks SCV Barracks SCV SCV SCV Marine Supply_Depot SCV Marine Refinery SCV Marine SCV Marine SCV Marine Supply_Depot SCV Academy";
		//str_build_order = "SCV SCV SCV SCV SCV Supply_Depot SCV Barracks Refinery SCV SCV SCV SCV Factory Factory SCV SCV SCV SCV Supply_Depot Siege_Tank_Siege_Mode Tank_Siege_Mode Siege_Tank_Tank_Mode Siege_Tank_Tank_Mode Siege_Tank_Tank_Mode";
		//str_build_order = "Academy Ghost";
	}
	else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss){
		_main_strategy = "Protoss_ZealotRush";
		str_build_order = "Probe Probe Probe Probe Pylon Probe Gateway Gateway Probe Probe Zealot Pylon Zealot Zealot";
	}
	else{
		_main_strategy = "Zerg_ZerglingRush";
		str_build_order = "Drone Spawning_Pool Zergling Zergling Zergling Zergling";
	}

	std::istringstream iss(str_build_order);

	std::vector<std::string> vec_build_order{ std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{} };

	for (auto &i : vec_build_order){
		_openingBuildOrder.add(MetaType(i));
	}

	//@도주남 김유진 전략별 유닛최대치 세팅
	_strategies["Terran_Bionic"] = Strategy();
	_strategies["Terran_Bionic"].pre_strategy_name = "None";
	_strategies["Terran_Bionic"].next_strategy_name = "Terran_Bionic_Tank";
	_strategies["Terran_Bionic"].num_unit_limit["Marines"] = 12; //마린18이상이면 테크진화
	//_strategies["Terran_Bionic"].num_unit_limit["Barracks"] = 4; //배럭4개 이상
	_strategies["Terran_Bionic"].num_unit_limit["Medics"] = -1; //-1은 테크진화에 영향없음
	_strategies["Terran_Bionic"].num_unit_limit["Firebats"] = -1;

	_strategies["Terran_Bionic_Tank"] = Strategy();
	_strategies["Terran_Bionic_Tank"].pre_strategy_name = "None";
	_strategies["Terran_Bionic_Tank"].next_strategy_name = "Terran_Mechanic";
	_strategies["Terran_Bionic_Tank"].num_unit_limit["Tanks"] = 5;

	_strategies["Terran_Mechanic"] = Strategy();
	_strategies["Terran_Mechanic"].pre_strategy_name = "None";
	_strategies["Terran_Mechanic"].next_strategy_name = "None";
	_strategies["Terran_Mechanic"].num_unit_limit["Tanks"] = 12; //마린18이상이면 테크진화

	_strategies["Terran_Mechanic_Goliath"] = Strategy();
	_strategies["Terran_Mechanic_Goliath"].pre_strategy_name = "None";
	_strategies["Terran_Mechanic_Goliath"].next_strategy_name = "None";
	_strategies["Terran_Mechanic_Goliath"].num_unit_limit["Tanks"] = 12; //마린18이상이면 테크진화

	//_strategies["Terran_Mechanic_Vessel"] = Strategy();
	//_strategies["Terran_Mechanic_Vessel"].pre_strategy_name = "None";
	//_strategies["Terran_Mechanic_Vessel"].next_strategy_name = "None";

	//@도주남 김유진 유닛비율 테이블
	std::cout << "Medic ratio" << std::endl;
	unit_ratio_table["Medics"].push_back(0);
	unit_ratio_table["Medics"].push_back(0); //index 1 부터 시작
	for (int i = 1; i < 201; i++){
		if (i%10 == 0)
			std::cout << unit_ratio_table["Medics"][i - 1] << " ";

		unit_ratio_table["Medics"].push_back(int(log(i) / log(1.5)));
	}
	std::cout << std::endl;
	
}

void StrategyManager::onEnd(bool isWinner)
{	
}

void StrategyManager::update()
{
	if (BuildManager::Instance().buildQueue.isEmpty()) {
		isInitialBuildOrderFinished = true;
	}
		
	//executeWorkerTraining();

	//executeSupplyManagement();

	//executeBasicCombatUnitTraining();

	//executeCombat();
}

const BuildOrder & StrategyManager::getOpeningBookBuildOrder() const
{
	return _openingBuildOrder;
}

// 일꾼 계속 추가 생산
void StrategyManager::executeWorkerTraining()
{
	// InitialBuildOrder 진행중에는 아무것도 하지 않습니다
	if (isInitialBuildOrderFinished == false) {
		return;
	}

	if (BWAPI::Broodwar->self()->minerals() >= 50) {
		// workerCount = 현재 일꾼 수 + 생산중인 일꾼 수
		int workerCount = BWAPI::Broodwar->self()->allUnitCount(InformationManager::Instance().getWorkerType());

		if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {

			for (auto & unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg) {
					// Zerg_Egg 에게 morph 명령을 내리면 isMorphing = true, isBeingConstructed = true, isConstructing = true 가 된다
					// Zerg_Egg 가 다른 유닛으로 바뀌면서 새로 만들어진 유닛은 잠시 isBeingConstructed = true, isConstructing = true 가 되었다가, 
					if (unit->isMorphing() && unit->getBuildType() == BWAPI::UnitTypes::Zerg_Drone) {
						workerCount++;
					}
				}
			}
		}
		else {
			for (auto & unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (unit->getType().isResourceDepot())
				{
					if (unit->isTraining()) {
						workerCount += unit->getTrainingQueue().size();
					}
				}
			}
		}

		if (workerCount < 30) {
			for (auto & unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (unit->getType().isResourceDepot())
				{
					if (unit->isTraining() == false || unit->getLarva().size() > 0) {

						// 빌드큐에 일꾼 생산이 1개는 있도록 한다
						if (BuildManager::Instance().buildQueue.getItemCount(InformationManager::Instance().getWorkerType()) == 0) {
							//std::cout << "worker enqueue" << std::endl;
							BuildManager::Instance().buildQueue.queueAsLowestPriority(MetaType(InformationManager::Instance().getWorkerType()), false);
						}
					}
				}
			}
		}
	}
}

// Supply DeadLock 예방 및 SupplyProvider 가 부족해질 상황 에 대한 선제적 대응으로서 SupplyProvider를 추가 건설/생산한다
void StrategyManager::executeSupplyManagement()
{
	// InitialBuildOrder 진행중에는 아무것도 하지 않습니다
	if (isInitialBuildOrderFinished == false) {
		return;
	}

	// 1초에 한번만 실행
	if (BWAPI::Broodwar->getFrameCount() % 24 != 0) {
		return;
	}

	// 게임에서는 서플라이 값이 200까지 있지만, BWAPI 에서는 서플라이 값이 400까지 있다
	// 저글링 1마리가 게임에서는 서플라이를 0.5 차지하지만, BWAPI 에서는 서플라이를 1 차지한다
	if (BWAPI::Broodwar->self()->supplyTotal() <= 400)
	{
		// 서플라이가 다 꽉찼을때 새 서플라이를 지으면 지연이 많이 일어나므로, supplyMargin (게임에서의 서플라이 마진 값의 2배)만큼 부족해지면 새 서플라이를 짓도록 한다
		// 이렇게 값을 정해놓으면, 게임 초반부에는 서플라이를 너무 일찍 짓고, 게임 후반부에는 서플라이를 너무 늦게 짓게 된다
		int supplyMargin = 12;

		// currentSupplyShortage 를 계산한다
		int currentSupplyShortage = BWAPI::Broodwar->self()->supplyUsed() + supplyMargin - BWAPI::Broodwar->self()->supplyTotal();

		if (currentSupplyShortage > 0) {

			// 생산/건설 중인 Supply를 센다
			int onBuildingSupplyCount = 0;

			// 저그 종족인 경우, 생산중인 Zerg_Overlord (Zerg_Egg) 를 센다. Hatchery 등 건물은 세지 않는다
			if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {
				for (auto & unit : BWAPI::Broodwar->self()->getUnits())
				{
					if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg && unit->getBuildType() == BWAPI::UnitTypes::Zerg_Overlord) {
						onBuildingSupplyCount += BWAPI::UnitTypes::Zerg_Overlord.supplyProvided();
					}
					// 갓태어난 Overlord 는 아직 SupplyTotal 에 반영안되어서, 추가 카운트를 해줘야함 
					if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord && unit->isConstructing()) {
						onBuildingSupplyCount += BWAPI::UnitTypes::Zerg_Overlord.supplyProvided();
					}
				}
			}
			// 저그 종족이 아닌 경우, 건설중인 Protoss_Pylon, Terran_Supply_Depot 를 센다. Nexus, Command Center 등 건물은 세지 않는다
			else {
				onBuildingSupplyCount += ConstructionManager::Instance().getConstructionQueueItemCount(InformationManager::Instance().getBasicSupplyProviderUnitType()) * InformationManager::Instance().getBasicSupplyProviderUnitType().supplyProvided();
			}

			std::cout << "currentSupplyShortage : " << currentSupplyShortage << " onBuildingSupplyCount : " << onBuildingSupplyCount << std::endl;

			if (currentSupplyShortage > onBuildingSupplyCount) {

				// BuildQueue 최상단에 SupplyProvider 가 있지 않으면 enqueue 한다
				bool isToEnqueue = true;
				if (!BuildManager::Instance().buildQueue.isEmpty()) {
					BuildOrderItem currentItem = BuildManager::Instance().buildQueue.getHighestPriorityItem();
					if (currentItem.metaType.isUnit() && currentItem.metaType.getUnitType() == InformationManager::Instance().getBasicSupplyProviderUnitType()) {
						isToEnqueue = false;
					}
				}
				if (isToEnqueue) {
					std::cout << "enqueue supply provider " << InformationManager::Instance().getBasicSupplyProviderUnitType().getName().c_str() << std::endl;
					BuildManager::Instance().buildQueue.queueAsHighestPriority(MetaType(InformationManager::Instance().getBasicSupplyProviderUnitType()), true);
				}
			}

		}
	}
}

void StrategyManager::executeBasicCombatUnitTraining()
{
	// InitialBuildOrder 진행중에는 아무것도 하지 않습니다
	if (isInitialBuildOrderFinished == false) {
		return;
	}

	// 기본 병력 추가 훈련
	if (BWAPI::Broodwar->self()->minerals() >= 200 && BWAPI::Broodwar->self()->supplyUsed() < 390) {
		{
			for (auto & unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (unit->getType() == InformationManager::Instance().getBasicCombatBuildingType()) {
					if (unit->isTraining() == false || unit->getLarva().size() > 0) {
						if (BuildManager::Instance().buildQueue.getItemCount(InformationManager::Instance().getBasicCombatUnitType()) == 0) {
							BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getBasicCombatUnitType());
						}
					}
				}
			}
		}
	}
}

const MetaPairVector StrategyManager::getBuildOrderGoal()
{
	BWAPI::Race myRace = BWAPI::Broodwar->self()->getRace();

	if (myRace == BWAPI::Races::Protoss)
	{
		return MetaPairVector();
	}
	else if (myRace == BWAPI::Races::Terran)
	{
		return getTerranBuildOrderGoal();
	}
	else
	{
		return MetaPairVector();
	}
}


bool StrategyManager::changeMainStrategy(std::map<std::string, int> & numUnits){
	//@도주남 김유진 다음 빌드오더로 변환체크
	std::cout << "main stragety:" << _main_strategy << " next_strategy:" << _strategies[_main_strategy].next_strategy_name << std::endl;
	std::cout << " num_marines:" << numUnits["Marines"] << " limit_marines:" << _strategies[_main_strategy].num_unit_limit["Marines"] << std::endl;
	if (_strategies[_main_strategy].next_strategy_name != "None"){
		bool _changeStrategy = true;
		for (auto &i : _strategies[_main_strategy].num_unit_limit){
			if (numUnits[i.first] < i.second){
				_changeStrategy = false;
				break;
			}
		}

		if (_changeStrategy){
			_main_strategy = _strategies[_main_strategy].next_strategy_name;
		}
	}

	//@도주남 김유진 이전 빌드오더로 변환체크
	if (_strategies[_main_strategy].pre_strategy_name != "None"){
		bool _changeStrategy = true;
		for (auto &i : _strategies[_main_strategy].num_unit_limit){
			if (i.second != -1 && numUnits[i.first] > i.second){
				_changeStrategy = false;
				break;
			}
		}

		if (_changeStrategy){
			_main_strategy = _strategies[_main_strategy].pre_strategy_name;
		}
	}

	if (_main_strategy == "Terran_Mechanic"){
		if (InformationManager::Instance().hasFlyingUnits){
			_main_strategy = "Terran_Mechanic_Goliath";
			return true;
		}
	}
	
	if (InformationManager::Instance().enemyRace == BWAPI::Races::Terran){

	}
	else if (InformationManager::Instance().enemyRace == BWAPI::Races::Protoss){

	}
	else if (InformationManager::Instance().enemyRace == BWAPI::Races::Zerg){

	}
	

	return false;
}

const MetaPairVector StrategyManager::getTerranBuildOrderGoal()
{
	// the goal to return
	std::vector<MetaPair> goal;

	std::map<std::string, int> numUnits;

	numUnits["Workers"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_SCV);
	numUnits["CC"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center);
	numUnits["Marines"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Marine);
	numUnits["Medics"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Medic);
	numUnits["Firebats"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Firebat);
	numUnits["Wraith"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Wraith);
	numUnits["Vultures"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Vulture);
	numUnits["Goliath"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Goliath);
	numUnits["Tanks"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);
	numUnits["Bay"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay);
	numUnits["Barracks"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Barracks);
	numUnits["Factorys"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Factory);
	
	std::cout << "changeMainStrategy : " << changeMainStrategy(numUnits) << " main strategy : " << _main_strategy << std::endl;

	if (_main_strategy == "Terran_MarineRush")
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, numUnits["Marines"] + 8));

		if (numUnits["Marines"] > 5 && numUnits["Bay"] == 0)
		{
			goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Engineering_Bay, 1));
		}
	}
	else if (_main_strategy == "Terran_Bionic")
	{
		int goal_num_marines = numUnits["Marines"] + 4;
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, goal_num_marines));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Medic, unit_ratio_table["Medics"][goal_num_marines]));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Firebat, unit_ratio_table["Medics"][goal_num_marines]));

		if (numUnits["Marines"] > 5 && numUnits["Bay"] == 0)
		{
			goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Engineering_Bay, 1));
		}

		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Stim_Packs, 1));
	}
	else if (_main_strategy == "Terran_Bionic_Tank")
	{
		int goal_num_marines = numUnits["Marines"]+numUnits["Barracks"];
		int goal_num_tanks = numUnits["Tanks"] + 2;
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, goal_num_marines));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Medic, unit_ratio_table["Medics"][goal_num_marines]));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Firebat, unit_ratio_table["Medics"][goal_num_marines]));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, goal_num_tanks));

		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
	}

	else if (_main_strategy == "Terran_Mechanic")
	{
		int goal_num_tanks = numUnits["Tanks"] + numUnits["Factorys"];
		//int goal_num_vultures = (goal_num_tanks - numUnits["Vultures"]) > numUnits["Factorys"] * 2 ? numUnits["Vultures"]+numUnits["Factorys"] : goal_num_tanks;
		//벌쳐는 탱크수만큼 뽑되 현재 탱크수와 벌쳐수가 많이 차이나면(팩토리의 2배만큼) 현재탱크수의 절반만 뽑는다.
		int goal_num_vultures = numUnits["Vultures"] + numUnits["Factorys"];

		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, goal_num_tanks));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Vulture, goal_num_vultures));
		
		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Spider_Mines, 1));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Ion_Thrusters, 1));
	}

	else if (_main_strategy == "Terran_Mechanic_Goliath")
	{
		int goal_num_tanks = numUnits["Tanks"] + 2;
		int goal_num_vultures = numUnits["Vultures"] + 2;
		int goal_num_goliath = numUnits["Goliath"] + 2;

		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, goal_num_tanks));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Vulture, goal_num_vultures));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Goliath, goal_num_goliath));

		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Spider_Mines, 1));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Ion_Thrusters, 1));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Charon_Boosters, 1));
	}

	else if (_main_strategy == "Terran_4RaxMarines")
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, numUnits["Marines"] + 8));
	}
	else if (_main_strategy == "Terran_VultureRush")
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Vulture, numUnits["Vultures"] + 8));

		if (numUnits["Vultures"] > 8)
		{
			goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
			goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, 4));
		}
	}
	else if (_main_strategy == "Terran_TankPush")
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, 6));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Goliath, numUnits["Goliath"] + 6));
		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
	}
	else
	{
		//BWAPI::Broodwar->printf("Warning: No build order goal for Terran Strategy: %s", Config::Strategy::StrategyName.c_str());
	}

	if (shouldExpandNow())
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Command_Center, numUnits["CC"] + 1));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_SCV, numUnits["Workers"] + 1));
	}

	return goal;
}

const bool StrategyManager::shouldExpandNow() const
{
	//@도주남 김유진 현재 커맨드센터 지어지고 있으면 그 때동안은 멀티 추가 안함
	for (auto &u : BWAPI::Broodwar->self()->getUnits()){
		if (u->getType() == BWAPI::UnitTypes::Terran_Command_Center && !u->isCompleted()){
			return false;
		}
	}
	// if there is no place to expand to, we can't expand
	if (MapTools::Instance().getNextExpansion() == BWAPI::TilePositions::None)
	{
		BWAPI::Broodwar->printf("No valid expansion location");
		return false;
	}

	size_t numDepots = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Protoss_Nexus)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hatchery)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Lair)
		+ UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hive);
	int frame = BWAPI::Broodwar->getFrameCount();
	int minute = frame / (24 * 60);

	// if we have a ton of idle workers then we need a new expansion
	if (WorkerManager::Instance().getNumIdleWorkers() > 7)
	{
		return true;
	}

	// if we have a ridiculous stockpile of minerals, expand
	if (BWAPI::Broodwar->self()->minerals() > 1700)
	{
		return true;
	}

	// we will make expansion N after array[N] minutes have passed
	std::vector<int> expansionTimes = { 5, 7, 13, 20, 40, 50 };

	for (size_t i(0); i < expansionTimes.size(); ++i)
	{
		if (numDepots < (i + 2) && minute > expansionTimes[i])
		{
			return true;
		}
	}

	return false;
}

//각 건물에 대한 설치전략
BuildOrderItem::SeedPositionStrategy StrategyManager::getBuildSeedPositionStrategy(MetaType type){
	//서플라이 5개까지만 본진커맨드 주변, 이후 본진 외곽
	if (type.getUnitType() == BWAPI::UnitTypes::Terran_Supply_Depot){
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Supply_Depot) > 5){
			std::cout << "build supply depot on MainBaseBackYard" << std::endl;
			return BuildOrderItem::SeedPositionStrategy::MainBaseBackYard;
		}
	}

	return BuildOrderItem::SeedPositionStrategy::MainBaseLocation;
}