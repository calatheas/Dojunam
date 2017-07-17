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
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran){
		setOpeningBookBuildOrder();
	}
}

void StrategyManager::setOpeningBookBuildOrder(){
	//전체 전략 세팅
	initStrategies();

	//초기 전략 선택
	if (InformationManager::Instance().enemyRace == BWAPI::Races::Terran){
		_main_strategy = Strategy::main_strategies::Bionic;
	}
	else if (InformationManager::Instance().enemyRace == BWAPI::Races::Zerg){
		_main_strategy = Strategy::main_strategies::Bionic;
	}
	else if (InformationManager::Instance().enemyRace == BWAPI::Races::Protoss){
		_main_strategy = Strategy::main_strategies::Bionic;
	}
	else{
		_main_strategy = Strategy::main_strategies::Bionic;
	}

	//초기 빌드 큐에 세팅
	std::istringstream iss(_strategies[_main_strategy].opening_build_order);
	std::vector<std::string> vec_build_order{ std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{} };
	for (auto &i : vec_build_order){
		_openingBuildOrder.add(MetaType(i));
	}

	//@도주남 김유진 유닛비율 테이블 - 마린대메딕 비율
	initUnitRatioTable();
}

void StrategyManager::onEnd(bool isWinner)
{	
}

void StrategyManager::update()
{
	//1. 초기 빌드는 onstart에서 세팅
	//2. todo) update에서 초기빌드 깨졌는지 확인
	//3. 초기빌드가 깨지거나 초기빌드를 다 사용하면 isInitialBuildOrderFinished 세팅하여 다이나믹 빌드오더 체제로 전환
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

const MetaPairVector StrategyManager::getBuildOrderGoal()
{
	//초기빌드 이후에만 작업가능
	if (!isInitialBuildOrderFinished) false;

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
	std::cout << "main stragety:" << _main_strategy << " next_strategy:" << _strategies[_main_strategy].next_strategy << std::endl;
	std::cout << " num_marines:" << numUnits["Marines"] << " limit_marines:" << _strategies[_main_strategy].num_unit_limit["Marines"] << std::endl;
	if (_strategies[_main_strategy].next_strategy != Strategy::main_strategies::None){
		bool _changeStrategy = true;
		for (auto &i : _strategies[_main_strategy].num_unit_limit){
			if (numUnits[i.first] < i.second){
				_changeStrategy = false;
				break;
			}
		}

		if (_changeStrategy){
			_main_strategy = _strategies[_main_strategy].next_strategy;
		}
	}

	//@도주남 김유진 이전 빌드오더로 변환체크
	if (_strategies[_main_strategy].pre_strategy != Strategy::main_strategies::None){
		bool _changeStrategy = true;
		for (auto &i : _strategies[_main_strategy].num_unit_limit){
			if (i.second != -1 && numUnits[i.first] > i.second){
				_changeStrategy = false;
				break;
			}
		}

		if (_changeStrategy){
			_main_strategy = _strategies[_main_strategy].pre_strategy;
		}
	}

	if (_main_strategy == Strategy::main_strategies::Mechanic){
		if (InformationManager::Instance().hasFlyingUnits){
			_main_strategy = Strategy::main_strategies::Mechanic_Goliath;
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

	if (_main_strategy == Strategy::main_strategies::Bionic)
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
	else if (_main_strategy == Strategy::main_strategies::Bionic_Tank)
	{
		int goal_num_marines = numUnits["Marines"]+numUnits["Barracks"];
		int goal_num_tanks = numUnits["Tanks"] + 2;
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, goal_num_marines));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Medic, unit_ratio_table["Medics"][goal_num_marines]));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Firebat, unit_ratio_table["Medics"][goal_num_marines]));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, goal_num_tanks));

		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
	}

	else if (_main_strategy == Strategy::main_strategies::Mechanic)
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

	else if (_main_strategy == Strategy::main_strategies::Mechanic_Goliath)
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

int StrategyManager::getUnitLimit(MetaType type){
	if (type.getUnitType() == BWAPI::UnitTypes::Terran_Barracks){
		return 4;
	}

	return -1;
}

void StrategyManager::initStrategies(){
	/*
	@도주남 김유진 전략구조체 세팅
	1. 전략이름정의 : enum
	2. 이전/다음 전략 세팅
	3. 초기빌드 세팅
	4. 이전/다음 전략 넘어가는 조건 세팅
	*/
	_strategies[Strategy::main_strategies::Bionic] = Strategy();
	_strategies[Strategy::main_strategies::Bionic].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Bionic].next_strategy = Strategy::main_strategies::Bionic_Tank;
	_strategies[Strategy::main_strategies::Bionic].opening_build_order = "SCV SCV SCV SCV SCV Supply_Depot SCV Barracks SCV Barracks SCV SCV SCV Marine Supply_Depot SCV Marine Refinery SCV Marine SCV Marine SCV Marine Supply_Depot SCV Academy";
	_strategies[Strategy::main_strategies::Bionic].num_unit_limit["Marines"] = 12; //마린18이상이면 테크진화
	_strategies[Strategy::main_strategies::Bionic].num_unit_limit["Medics"] = -1; //-1은 테크진화에 영향없음
	_strategies[Strategy::main_strategies::Bionic].num_unit_limit["Firebats"] = -1;

	_strategies[Strategy::main_strategies::Bionic_Tank] = Strategy();
	_strategies[Strategy::main_strategies::Bionic_Tank].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Bionic_Tank].next_strategy = Strategy::main_strategies::Mechanic;
	_strategies[Strategy::main_strategies::Bionic_Tank].opening_build_order = "SCV SCV SCV SCV SCV Supply_Depot SCV Barracks SCV Barracks SCV SCV SCV Marine Supply_Depot SCV Marine Refinery SCV Marine SCV Marine SCV Marine Supply_Depot SCV Academy";
	_strategies[Strategy::main_strategies::Bionic_Tank].num_unit_limit["Tanks"] = 5;

	_strategies[Strategy::main_strategies::Mechanic] = Strategy();
	_strategies[Strategy::main_strategies::Mechanic].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Mechanic].next_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Mechanic].opening_build_order = "SCV SCV SCV SCV SCV Supply_Depot SCV Barracks SCV Barracks SCV SCV SCV Marine Supply_Depot SCV Marine Refinery SCV Marine SCV Marine SCV Marine Supply_Depot SCV Academy";
	_strategies[Strategy::main_strategies::Mechanic].num_unit_limit["Tanks"] = 12; //마린18이상이면 테크진화

	_strategies[Strategy::main_strategies::Mechanic_Goliath] = Strategy();
	_strategies[Strategy::main_strategies::Mechanic_Goliath].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Mechanic_Goliath].next_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Mechanic_Goliath].opening_build_order = "SCV SCV SCV SCV SCV Supply_Depot SCV Barracks SCV Barracks SCV SCV SCV Marine Supply_Depot SCV Marine Refinery SCV Marine SCV Marine SCV Marine Supply_Depot SCV Academy";
	_strategies[Strategy::main_strategies::Mechanic_Goliath].num_unit_limit["Tanks"] = 12; //마린18이상이면 테크진화
}
void StrategyManager::initUnitRatioTable(){
	//@도주남 김유진 유닛비율 테이블
	std::cout << "Medic ratio" << std::endl;
	unit_ratio_table["Medics"].push_back(0);
	unit_ratio_table["Medics"].push_back(0); //index 1 부터 시작
	for (int i = 1; i < 201; i++){
		if (i % 10 == 0)
			std::cout << unit_ratio_table["Medics"][i - 1] << " ";

		unit_ratio_table["Medics"].push_back(int(log(i) / log(1.5)));
	}
	std::cout << std::endl;
}