#include "GameCommander.h"
#include <direct.h>

using namespace MyBot;

GameCommander::GameCommander(){
	isToFindError = true;
	//@도주남 김지훈
	_initialScoutSet = false;

	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	//tstruct = *localtime(&now);
	localtime_s(&tstruct, &now);
	strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &tstruct);

	log_file_path = Config::Strategy::WriteDir + std::string(buf) + ".log";
	std::cout << "log_file_path:" << log_file_path << std::endl;
	
}
GameCommander::~GameCommander(){
}

void GameCommander::onStart() 
{
	BWAPI::TilePosition startLocation = BWAPI::Broodwar->self()->getStartLocation();
	if (startLocation == BWAPI::TilePositions::None || startLocation == BWAPI::TilePositions::Unknown) {
		return;
	}

	//맵정보 세팅
	InformationManager::Instance().onStart();

	//초기화용
	BOSS::init();

	//초기빌드 세팅
	StrategyManager::Instance().onStart();

	//맵정보에 따른 resourcedepot 당 일꾼 최대수 결정
	BuildManager::Instance().onStart();

	//맵정보 초기세팅(리젼별 경계세팅)
	MapGrid::Instance().onStart();

	//리젼 정보 확인용
	//BWAPI의 region과 BWTA의 region은 완전 다름
	/*
	for (auto &r : BWTA::getRegions()){
		std::cout << r->getCenter() << "/" << r->getPolygon().getArea() << std::endl;
	}

	for (auto &u : BWAPI::Broodwar->self()->getUnits()){
		BWTA::Region *r = BWTA::getRegion(u->getPosition());
		std::cout << "my base-" << r->getCenter() << "/" << r->getPolygon().getArea() << std::endl;
	}
	for (auto &u : BWAPI::Broodwar->self()->getUnits()){
		std::cout << u->getType() << ":" << u->getType().width() << "," << u->getType().height() << "/" << u->getPosition() << std::endl;
	}
	*/
}
void GameCommander::onEnd(bool isWinner)
{
	StrategyManager::Instance().onEnd(isWinner);
}

void GameCommander::onFrame()
{
	if (BWAPI::Broodwar->isPaused() 
		|| BWAPI::Broodwar->self() == nullptr || BWAPI::Broodwar->self()->isDefeated() || BWAPI::Broodwar->self()->leftGame()
		|| BWAPI::Broodwar->enemy() == nullptr || BWAPI::Broodwar->enemy()->isDefeated() || BWAPI::Broodwar->enemy()->leftGame()) {
		return;
	}
	log_write(BWAPI::Broodwar->getFrameCount());

	// 아군 베이스 위치. 적군 베이스 위치. 각 유닛들의 상태정보 등을 Map 자료구조에 저장/업데이트
	InformationManager::Instance().update();

	log_write(":InformationManager, ");

	// 각 유닛의 위치를 자체 MapGrid 자료구조에 저장
	MapGrid::Instance().update();
		
	log_write("MapGrid, ");

	BOSSManager::Instance().update(49.0); //순서가 중요?

	log_write("BOSSManager, ");

	// economy and base managers
	// 일꾼 유닛에 대한 명령 (자원 채취, 이동 정도) 지시 및 정리
	WorkerManager::Instance().update();

	log_write("WorkerManager, ");

	// 빌드오더큐를 관리하며, 빌드오더에 따라 실제 실행(유닛 훈련, 테크 업그레이드 등)을 지시한다.
	//BuildManager::Instance().update();

	log_write("BuildManager, ");

	// 빌드오더 중 건물 빌드에 대해서는, 일꾼유닛 선정, 위치선정, 건설 실시, 중단된 건물 빌드 재개를 지시한다
	//ConstructionManager::Instance().update();

	log_write("ConstructionManager, ");

	// 게임 초기 정찰 유닛 지정 및 정찰 유닛 컨트롤을 실행한다
	ScoutManager::Instance().update();

	log_write("ScoutManager, ");

	// 전략적 판단 및 유닛 컨트롤
	StrategyManager::Instance().update();

	log_write("StrategyManager, ");

	//@도주남 김지훈 전투유닛 셋팅
	handleUnitAssignments();
	CombatCommander::Instance().update(_combatUnits);

	log_write("CombatCommander, ");

	ComsatManager::Instance().update();

	log_write("ComsatManager, ");

	ExpansionManager::Instance().update(); //본진 및 확장정보 저장, 가스/컴셋 주기적으로 생성

	log_write("ExpansionManager");
}

// BasicBot 1.1 Patch Start ////////////////////////////////////////////////
// 일꾼 탄생/파괴 등에 대한 업데이트 로직 버그 수정 : onUnitShow 가 아니라 onUnitComplete 에서 처리하도록 수정
void GameCommander::onUnitShow(BWAPI::Unit unit)			
{ 
	InformationManager::Instance().onUnitShow(unit); 

	// ResourceDepot 및 Worker 에 대한 처리
	WorkerManager::Instance().onUnitShow(unit);
}

// BasicBot 1.1 Patch End //////////////////////////////////////////////////

void GameCommander::onUnitHide(BWAPI::Unit unit)			
{
	InformationManager::Instance().onUnitHide(unit); 
}

void GameCommander::onUnitCreate(BWAPI::Unit unit)
{ 
	InformationManager::Instance().onUnitCreate(unit);
}

// BasicBot 1.1 Patch Start ////////////////////////////////////////////////
// 일꾼 탄생/파괴 등에 대한 업데이트 로직 버그 수정 : onUnitShow 가 아니라 onUnitComplete 에서 처리하도록 수정
void GameCommander::onUnitComplete(BWAPI::Unit unit)
{
	InformationManager::Instance().onUnitComplete(unit);
	ExpansionManager::Instance().onUnitComplete(unit); //본진 및 확장정보 저장, 가스/컴셋 주기적으로 생성
	BuildManager::Instance().onUnitComplete(unit);

	// ResourceDepot 및 Worker 에 대한 처리
	WorkerManager::Instance().onUnitComplete(unit);
}

// BasicBot 1.1 Patch End //////////////////////////////////////////////////

void GameCommander::onUnitDestroy(BWAPI::Unit unit)
{
	InformationManager::Instance().onUnitDestroy(unit);
	ExpansionManager::Instance().onUnitDestroy(unit); //본진 및 확장정보 저장, 가스/컴셋 주기적으로 생성
	// ResourceDepot 및 Worker 에 대한 처리
	WorkerManager::Instance().onUnitDestroy(unit);

	if (_scoutUnits.contains(unit)) { _scoutUnits.erase(unit); }
}

void GameCommander::onUnitRenegade(BWAPI::Unit unit)
{
	// Vespene_Geyser (가스 광산) 에 누군가가 건설을 했을 경우
	//BWAPI::Broodwar->sendText("A %s [%p] has renegaded. It is now owned by %s", unit->getType().c_str(), unit, unit->getPlayer()->getName().c_str());

	InformationManager::Instance().onUnitRenegade(unit);
}

void GameCommander::onUnitMorph(BWAPI::Unit unit)
{ 
	InformationManager::Instance().onUnitMorph(unit);

	// Zerg 종족 Worker 의 Morph 에 대한 처리
	WorkerManager::Instance().onUnitMorph(unit);
}

void GameCommander::onUnitDiscover(BWAPI::Unit unit)
{
}

void GameCommander::onUnitEvade(BWAPI::Unit unit)
{
}

// BasicBot 1.1 Patch Start ////////////////////////////////////////////////
// onNukeDetect, onPlayerLeft, onSaveGame 이벤트를 처리할 수 있도록 메소드 추가

void GameCommander::onNukeDetect(BWAPI::Position target)
{
}

void GameCommander::onPlayerLeft(BWAPI::Player player)
{
}

void GameCommander::onSaveGame(std::string gameName)
{
}

// BasicBot 1.1 Patch End //////////////////////////////////////////////////

void GameCommander::onSendText(std::string text)
{
	Kyj::Instance().onSendText(text);
}

void GameCommander::onReceiveText(BWAPI::Player player, std::string text)
{	
}

//@도주남 김지훈 
void GameCommander::handleUnitAssignments()
{
	//_validUnits.clear();
	_combatUnits.clear();

	// filter our units for those which are valid and usable
	//setValidUnits();

	// set each type of unit
	//djn ssh
	setScoutUnits();
	setCombatUnits();
}
//djn ssh

void GameCommander::setScoutUnits()
{
	// if we haven't set a scout unit, do it
	if (_scoutUnits.empty() && !_initialScoutSet)
	{
		BWAPI::Unit supplyProvider;// = getFirstSupplyProvider();

		bool flag = false;
		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType().isBuilding() == true && unit->getType().isResourceDepot() == false)
			{
				supplyProvider = unit;
				flag = true;
				break;
			}
		}
		// if it exists
		//std::cout << "error??" << std::endl;
		if (flag)
		{
			// grab the closest worker to the supply provider to send to scout
			BWAPI::Unit workerScout = WorkerManager::Instance().getClosestMineralWorkerTo(supplyProvider->getPosition());
			//	getClosestWorkerToTarget(supplyProvider->getPosition());

			// if we find a worker (which we should) add it to the scout units
			if (workerScout)
			{
				ScoutManager::Instance().setWorkerScout(workerScout);
				assignUnit(workerScout, _scoutUnits);
				_initialScoutSet = true;
			}
		}
		//std::cout << "Lucky??" << std::endl;
	}
	else if (_scoutUnits.empty() && _initialScoutSet == true && ScoutManager::Instance().second_scout == false)
	{
		BWAPI::Unit workerScout = nullptr;


		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (!unit)
			{
				continue;
			}

			if (unit->isCompleted()
				&& unit->getHitPoints() > 0
				&& unit->exists()
				&& unit->getType().isWorker()
				&& !unit->isCarryingMinerals()
				&& WorkerManager::Instance().isMineralWorker(unit))
			{
				workerScout = unit;
				break;
			}
		}
		//	getClosestWorkerToTarget(supplyProvider->getPosition());

		// if we find a worker (which we should) add it to the scout units
		if (workerScout)
		{
			ScoutManager::Instance().setWorkerScout(workerScout);
			assignUnit(workerScout, _scoutUnits);
			ScoutManager::Instance().second_scout = true;
		}
	}
}

//@도주남 김지훈 // 전투유닛을 setting 해주는 부분 기존 로직과 다르게 적용함.
void GameCommander::setCombatUnits()
{
	int combatunitCount = 0;
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (_scoutUnits.contains(unit) || unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
			continue;

		if (UnitUtil::IsValidUnit(unit))
			if (UnitUtil::IsCombatUnit(unit) && !unit->getType().isWorker())
			{
				//unit->getOrder
				BWAPI::UnitCommand currentCommand(unit->getLastCommand());

				//@도주남 김지훈 일꾼이 아니면 넣는다.
				//if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Move)
				{
					assignUnit(unit, _combatUnits);
					combatunitCount++;
				}
			}
	}
	//if (combatunitCount!=0)
	//	std::cout << "공격 유닛 [" << combatunitCount  <<"]명 셋팅 !" << std::endl;
}


void GameCommander::assignUnit(BWAPI::Unit unit, BWAPI::Unitset & set)
{
	//@도주남 김지훈 다른 유닛set에 포함되였는지를 확인하고 제거해주는 로직이 존재 하지만, 지금 전투유닛만 쓸꺼니까 필요없음;
	//    if (_scoutUnits.contains(unit)) { _scoutUnits.erase(unit); }
	//    else if (_combatUnits.contains(unit)) { _combatUnits.erase(unit); }

	set.insert(unit);
}
