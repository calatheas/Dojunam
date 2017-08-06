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
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
	{
		setOpeningBookBuildOrder();
	}
}

void StrategyManager::setOpeningBookBuildOrder(){
	//전체 전략 세팅
	initStrategies();

	//초기 전략 선택
	if (InformationManager::Instance().enemyRace == BWAPI::Races::Terran){
		_main_strategy = Strategy::main_strategies::One_Fac;
	}
	else if (InformationManager::Instance().enemyRace == BWAPI::Races::Zerg){
		_main_strategy = Strategy::main_strategies::BSB;
	}
	else if (InformationManager::Instance().enemyRace == BWAPI::Races::Protoss){
		//_main_strategy = Strategy::main_strategies::Mechanic;
		//_main_strategy = Strategy::main_strategies::Two_Fac;
		//_main_strategy = Strategy::main_strategies::One_Fac_Tank;
		_main_strategy = Strategy::main_strategies::One_Fac;
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

	if (_main_strategy == Strategy::main_strategies::Mechanic || _main_strategy == Strategy::main_strategies::One_Fac || _main_strategy == Strategy::main_strategies::Two_Fac){
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
	numUnits["Academy"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Academy);
	numUnits["Science_Facility"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Science_Facility);
	numUnits["Dropships"] = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Dropship);

	std::cout << "changeMainStrategy : " << changeMainStrategy(numUnits) << " main strategy : " << _main_strategy << std::endl;

	if (_main_strategy == Strategy::main_strategies::BSB || _main_strategy == Strategy::main_strategies::BBS)
	{
		int goal_num_marines = numUnits["Marines"] + 2;
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, goal_num_marines));

	}
	else if (_main_strategy == Strategy::main_strategies::Bionic)
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
		int goal_num_marines = numUnits["Marines"] + numUnits["Barracks"];
		int goal_num_tanks = numUnits["Tanks"] + 2;
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, goal_num_marines));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Medic, unit_ratio_table["Medics"][goal_num_marines]));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Firebat, unit_ratio_table["Medics"][goal_num_marines]));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, goal_num_tanks));

		goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
	}
	else if (_main_strategy == Strategy::main_strategies::One_Fac) {
		int goal_num_marines = numUnits["Marines"] + 1;
		int goal_num_vultures = numUnits["Vultures"];
		int goal_num_tanks = numUnits["Tanks"];

		if (numUnits["Vultures"] > numUnits["Tanks"]) {
			goal_num_tanks += 1;
		}
		else {
			goal_num_vultures += 1;
		}
		
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Marine, goal_num_marines));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Vulture, goal_num_vultures));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, goal_num_tanks));

		if (!hasTech(BWAPI::TechTypes::Tank_Siege_Mode)) {
			goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
		}
		if (hasTech(BWAPI::TechTypes::Tank_Siege_Mode) && !hasTech(BWAPI::TechTypes::Spider_Mines)) {
			goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Spider_Mines, 1));
		}
	}
	else if (_main_strategy == Strategy::main_strategies::Two_Fac)
	{
		int goal_num_vultures = numUnits["Vultures"] + 1;
		int goal_num_tanks = numUnits["Tanks"] + 1;
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Vulture, goal_num_vultures));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, goal_num_tanks));

		if (!hasTech(BWAPI::TechTypes::Tank_Siege_Mode)) {
			goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
		}
		if (hasTech(BWAPI::TechTypes::Tank_Siege_Mode) && !hasTech(BWAPI::TechTypes::Spider_Mines)) {
			goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Spider_Mines, 1));
		}
	}
	else if (_main_strategy == Strategy::main_strategies::Mechanic || _main_strategy == Strategy::main_strategies::Mechanic_Goliath)
	{
		int goal_num_tanks = numUnits["Tanks"];
		int goal_num_vultures = numUnits["Vultures"];
		int goal_num_goliath = numUnits["Goliath"];;

		if (numUnits["Factorys"] > 1 && numUnits["Academy"] == 0)
		{
			goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Academy, 1));
		}
		if (numUnits["Factorys"] > 3)
		{
			goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons, BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons) + 1));
			goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Terran_Vehicle_Plating, BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Plating) + 1));
			goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Dropship, 1));

		}

		if (numUnits["Factorys"] > 5 && numUnits["Science_Facility"] == 0) {
			goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Science_Facility, 1));
		}

		if (_main_strategy == Strategy::main_strategies::Mechanic) {
			if (BWAPI::Broodwar->self()->gas() < 500) {
				goal_num_vultures += numUnits["Factorys"];
			}
			else {
				if (goal_num_tanks > goal_num_vultures){
					goal_num_vultures += (goal_num_tanks - goal_num_vultures > numUnits["Factorys"] ? numUnits["Factorys"] : goal_num_tanks - goal_num_vultures);
					goal_num_tanks += ((numUnits["Factorys"] - (goal_num_tanks - goal_num_vultures)) > 0 ? (numUnits["Factorys"] - (goal_num_tanks - goal_num_vultures)) : 0);
				}
				else {
					goal_num_tanks += (goal_num_vultures - goal_num_tanks > numUnits["Factorys"] ? numUnits["Factorys"] : goal_num_vultures - goal_num_tanks);
					goal_num_vultures += ((numUnits["Factorys"] - (goal_num_vultures - goal_num_tanks)) > 0 ? (numUnits["Factorys"] - (goal_num_vultures - goal_num_tanks)) : 0);
				}

			}
		}
		else {
			if (BWAPI::Broodwar->self()->gas() < 500) {
				goal_num_vultures += numUnits["Factorys"];
			}
			else{
				if (goal_num_tanks > goal_num_goliath){
					goal_num_goliath += (goal_num_tanks - goal_num_goliath > numUnits["Factorys"] ? numUnits["Factorys"] : goal_num_tanks - goal_num_goliath);
					goal_num_tanks += ((numUnits["Factorys"] - (goal_num_tanks - goal_num_goliath)) > 0 ? (numUnits["Factorys"] - (goal_num_tanks - goal_num_goliath)) : 0);
				}
				else {
					goal_num_tanks += (goal_num_goliath - goal_num_tanks > numUnits["Factorys"] ? numUnits["Factorys"] : goal_num_goliath - goal_num_tanks);
					goal_num_goliath += ((numUnits["Factorys"] - (goal_num_goliath - goal_num_tanks)) > 0 ? (numUnits["Factorys"] - (goal_num_goliath - goal_num_tanks)) : 0);
				}
				goal_num_vultures += 1;
			}
		}

		if (!hasTech(BWAPI::TechTypes::Tank_Siege_Mode)) {
			goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Tank_Siege_Mode, 1));
		}
		if (hasTech(BWAPI::TechTypes::Tank_Siege_Mode) && !hasTech(BWAPI::TechTypes::Spider_Mines)) {
			goal.push_back(std::pair<MetaType, int>(BWAPI::TechTypes::Spider_Mines, 1));
		}
		if (hasTech(BWAPI::TechTypes::Spider_Mines) && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Ion_Thrusters) == 0) {
			goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Ion_Thrusters, 1));
		}
		if (_main_strategy == Strategy::main_strategies::Mechanic_Goliath && BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Charon_Boosters) == 0) {
			goal.push_back(std::pair<MetaType, int>(BWAPI::UpgradeTypes::Charon_Boosters, 1));
		}

		//int goal_num_vultures = (goal_num_tanks - numUnits["Vultures"]) > numUnits["Factorys"] * 2 ? numUnits["Vultures"]+numUnits["Factorys"] : goal_num_tanks;
		//벌쳐는 탱크수만큼 뽑되 현재 탱크수와 벌쳐수가 많이 차이나면(팩토리의 2배만큼) 현재탱크수의 절반만 뽑는다.
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Vulture, goal_num_vultures));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Goliath, goal_num_goliath));
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, goal_num_tanks));

	}
	else
	{
		//BWAPI::Broodwar->printf("Warning: No build order goal for Terran Strategy: %s", Config::Strategy::StrategyName.c_str());
	}

	if (ExpansionManager::Instance().shouldExpandNow())
	{
		goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_Command_Center, numUnits["CC"] + 1));
		//goal.push_back(std::pair<MetaType, int>(BWAPI::UnitTypes::Terran_SCV, numUnits["Workers"] + 1));
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
	//TODO : 큐 단위 이므로 큐에 있는것까지 고려는 잘 안됨.
	//서플라이 2개까지만 본진커맨드 주변, 이후 본진과 쵸크포인트 둘다 먼곳
	if (type.getUnitType() == BWAPI::UnitTypes::Terran_Supply_Depot){
		if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Supply_Depot) > 3){
			std::cout << "build supply depot on MainBaseOppositeChock" << std::endl;
			return BuildOrderItem::SeedPositionStrategy::MainBaseOppositeChock;
		}
	}

	if (!ExpansionManager::Instance().getExpansions().empty() && ExpansionManager::Instance().getExpansions()[0].complexity > 0.2){
		std::cout << "build " << type.getName() << " on LowComplexityExpansionLocation" << std::endl;
		return BuildOrderItem::SeedPositionStrategy::LowComplexityExpansionLocation;
	}
	return BuildOrderItem::SeedPositionStrategy::MainBaseLocation;
}

int StrategyManager::getUnitLimit(MetaType type){
	if (type.getUnitType() == BWAPI::UnitTypes::Terran_Barracks) {
		if (_main_strategy == Strategy::main_strategies::BSB || _main_strategy == Strategy::main_strategies::BBS) {
			return 1;
		}
		else if (_main_strategy == Strategy::main_strategies::Bionic) {
			return 3;
		}
		else if (_main_strategy == Strategy::main_strategies::One_Fac || _main_strategy == Strategy::main_strategies::Two_Fac) {
			return 0;
		}
	}
	if (type.getUnitType() == BWAPI::UnitTypes::Terran_Factory){
		if (_main_strategy == Strategy::main_strategies::One_Fac) {
			return 0;
		}
		if (_main_strategy == Strategy::main_strategies::Two_Fac) {
			return 1;
		}
		return 7;
	}
	if (type.getUnitType() == BWAPI::UnitTypes::Terran_Armory) {
		return 1;
	}
	if (type.getUnitType() == BWAPI::UnitTypes::Terran_Engineering_Bay) {
		return 1;
	}
	if (type.getUnitType() == BWAPI::UnitTypes::Terran_Science_Facility) {
		return 0;
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

	_strategies[Strategy::main_strategies::BSB] = Strategy();
	_strategies[Strategy::main_strategies::BSB].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::BSB].next_strategy = Strategy::main_strategies::One_Fac;
	_strategies[Strategy::main_strategies::BSB].opening_build_order = "SCV SCV SCV SCV SCV Barracks SCV Supply_Depot Barracks";
	_strategies[Strategy::main_strategies::BSB].num_unit_limit["Marines"] = 8; //마린18이상이면 테크진화

	_strategies[Strategy::main_strategies::BBS] = Strategy();
	_strategies[Strategy::main_strategies::BBS].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::BBS].next_strategy = Strategy::main_strategies::One_Fac;
	_strategies[Strategy::main_strategies::BBS].opening_build_order = "SCV SCV SCV SCV SCV Barracks SCV Barracks Supply_Depot";
	_strategies[Strategy::main_strategies::BBS].num_unit_limit["Marines"] = 8; //마린18이상이면 테크진화

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

	_strategies[Strategy::main_strategies::One_Fac] = Strategy();
	_strategies[Strategy::main_strategies::One_Fac].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::One_Fac].next_strategy = Strategy::main_strategies::Two_Fac;
	//_strategies[Strategy::main_strategies::One_Fac_Vulture].opening_build_order = "SCV SCV SCV SCV SCV Supply_Depot SCV SCV Barracks Refinery SCV Marine SCV Marine Factory Supply_Depot";
	_strategies[Strategy::main_strategies::One_Fac].opening_build_order = "SCV SCV SCV SCV SCV Barracks SCV Refinery Supply_Depot SCV Marine Factory SCV Marine Supply_Depot";
	_strategies[Strategy::main_strategies::One_Fac].num_unit_limit["Tanks"] = 2;

	_strategies[Strategy::main_strategies::Two_Fac] = Strategy();
	_strategies[Strategy::main_strategies::Two_Fac].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Two_Fac].next_strategy = Strategy::main_strategies::Mechanic;
	_strategies[Strategy::main_strategies::Two_Fac].num_unit_limit["Tanks"] = 6;

	_strategies[Strategy::main_strategies::Mechanic] = Strategy();
	_strategies[Strategy::main_strategies::Mechanic].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Mechanic].next_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Mechanic].opening_build_order = "SCV SCV SCV SCV SCV Supply_Depot SCV SCV Barracks Refinery SCV SCV SCV Marine Supply_Depot Factory SCV Marine SCV Marine Machine_Shop Marine SCV Marine SCV Supply_Depot Siege_Tank_Tank_Mode Tank_Siege_Mode Marine SCV Marine SCV Supply_Depot Siege_Tank_Tank_Mode SCV Engineering_Bay Siege_Tank_Tank_Mode Command_Center Siege_Tank_Tank_Mode Supply_Depot Factory SCV SCV Siege_Tank_Tank_Mode";

	_strategies[Strategy::main_strategies::Mechanic_Goliath] = Strategy();
	_strategies[Strategy::main_strategies::Mechanic_Goliath].pre_strategy = Strategy::main_strategies::None;
	_strategies[Strategy::main_strategies::Mechanic_Goliath].next_strategy = Strategy::main_strategies::None;
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

bool StrategyManager::hasTech(BWAPI::TechType tech){
	BWAPI::Player self = BWAPI::Broodwar->self();
	return self->hasResearched(tech);
}

bool StrategyManager::obtainNextUpgrade(BWAPI::UpgradeType upgType)
{
	BWAPI::Player self = BWAPI::Broodwar->self();
	int maxLvl = self->getMaxUpgradeLevel(upgType);
	int currentLvl = self->getUpgradeLevel(upgType);
	if (!self->isUpgrading(upgType) && currentLvl < maxLvl &&
		self->completedUnitCount(upgType.whatsRequired(currentLvl + 1)) > 0 &&
		self->completedUnitCount(upgType.whatUpgrades()) > 0) {

	}
	//return self->getUnits(). upgrade(upgType);
	return false;
}

double StrategyManager::weightByFrame(double max_weight){
	int early = 7200; //초반은 7200까지

	return BWAPI::Broodwar->getFrameCount() > early ? 1.0 : ((1 - max_weight) * BWAPI::Broodwar->getFrameCount() / early) + max_weight;
}

Strategy::main_strategies StrategyManager::getMainStrategy() {
	return _main_strategy;
}
