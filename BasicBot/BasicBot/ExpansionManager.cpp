#include "ExpansionManager.h"

using namespace MyBot;

ExpansionManager::ExpansionManager()
	: startPositionDestroyed(false)
{

}

ExpansionManager & ExpansionManager::Instance()
{
	static ExpansionManager instance;
	return instance;
}
void ExpansionManager::onSendText(std::string text){

}

const std::vector<Expansion> & ExpansionManager::getExpansions(){
	return expansions;
}

// 유닛이 파괴/사망한 경우, 해당 유닛 정보를 삭제한다
void ExpansionManager::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getPlayer() == BWAPI::Broodwar->self())
	{
		//본진 및 멀티 커맨드센터 관리
		if (unit->getType() == BWAPI::UnitTypes::Terran_Command_Center){
			for (size_t i = 0; i < expansions.size(); i++){
				if (unit->getID() == expansions[i].cc->getID()){
					expansions.erase(expansions.begin() + i);
					WorkerManager::Instance().getWorkerData().removeDepot(unit);
					
					if (i == 0){
						startPositionDestroyed = true;
						std::cout << "Self start location is destroyed " << std::endl;
					}

					std::cout << "onUnitDestroy numExpansion:" << expansions.size() << std::endl;
					break;
				}
			}
		}

		//아군 건물은 혼잡도 계산을 한다. (단, 커맨드센터 파괴시에는 혼잡도 자체가 삭제되므로 뺀다.)
		if (unit->getType().isBuilding()){
			changeComplexity(unit, false); //decrease complexity;
		}
	}
}

void ExpansionManager::onUnitComplete(BWAPI::Unit unit){
	if (unit->getPlayer() == BWAPI::Broodwar->self()){
		if(unit->getType() == BWAPI::UnitTypes::Terran_Command_Center){
			//numExpansion는 본진 포함개수
			for (auto &unit_in_region : unit->getUnitsInRadius(400)){
				if (unit_in_region->getType() == BWAPI::UnitTypes::Resource_Mineral_Field){
					expansions.push_back(Expansion(unit));
					std::cout << "onUnitComplete numExpansion:" << expansions.size() << std::endl;
					break;
				}
			}
		}
		
		//아군 건물은 혼잡도 계산을 한다.
		if(unit->getType().isBuilding()){
			changeComplexity(unit); //increase complexity;
		}
	}

	//적이 점령했던 지역을 저장함. 단 업데이트는 어려움.(해처리를 여러개 지을수도 있어서)
	else{
		if (unit->getType() == InformationManager::Instance().enemyResourceDepotType){
			BWTA::Region *p_region = BWTA::getRegion(unit->getPosition());
			auto & it = enemyResourceRegions.find(p_region);
			if (it == enemyResourceRegions.end())
			{
				std::cout << "insert the enemy resource region:" << enemyResourceRegions.size() << std::endl;
				enemyResourceRegions.insert(p_region);
			}
		}
	}
}

void ExpansionManager::update(){
	if (!StrategyManager::Instance().isInitialBuildOrderFinished){
		return;
	}

	//1초에 4번
	if (BWAPI::Broodwar->getFrameCount() % 6 == 0) {
		BWAPI::Unit target;

		for (auto &e : expansions){
			bool enemyExists = false;
			bool refineryExists = true;
			bool comsatExists = false;

			if (BuildManager::Instance().hasAddon(e.cc)){
				comsatExists = true;
			}

			for (auto u : e.cc->getUnitsInRadius(300)){
				if (u->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser){
					target = u;
					refineryExists = false;
				}
				else if (u->getPlayer() == InformationManager::Instance().enemyPlayer){
					enemyExists = true;
					break;
				}
			}

			//이미 큐에 있으면 제외함
			//컴셋은 아카데미 필요함
			//빌드시작하여 컨스트럭트 큐에 있는지도 확인해야됨
			if (!enemyExists){
				if (!refineryExists){
					
					if (!BuildManager::Instance().hasUnitInQueue(BWAPI::UnitType(BWAPI::UnitTypes::Terran_Refinery)) &&
						(ConstructionManager::Instance().getConstructionQueueItemCount(BWAPI::UnitTypes::Terran_Refinery) == 0) &&
						(StrategyManager::Instance().getMainStrategy() != Strategy::BBS && StrategyManager::Instance().getMainStrategy() != Strategy::BSB)){
						BuildManager::Instance().addBuildOrderOneItem(MetaType(BWAPI::UnitTypes::Terran_Refinery), target->getInitialTilePosition());
					}
					/*
					if (!BuildManager::Instance().hasUnitInQueue(BWAPI::UnitType(BWAPI::UnitTypes::Terran_Refinery)) &&
						(ConstructionManager::Instance().getConstructionQueueItemCount(BWAPI::UnitTypes::Terran_Refinery) == 0)){
						BuildManager::Instance().addBuildOrderOneItem(MetaType(BWAPI::UnitTypes::Terran_Refinery), target->getInitialTilePosition());
					}
					*/
				}
				if (!comsatExists){

					if ((BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0) &&
						!BuildManager::Instance().hasUnitInQueue(BWAPI::UnitType(BWAPI::UnitTypes::Terran_Comsat_Station)) &&
							BWAPI::Broodwar->self()->gatheredGas() > 0){
						BuildManager::Instance().addBuildOrderOneItem(MetaType(BWAPI::UnitTypes::Terran_Comsat_Station));
					}	
				}
			}
		}
	}
}


bool ExpansionManager::shouldExpandNow()
{
	//@도주남 김유진 현재 커맨드센터 지어지고 있으면 그 때동안은 멀티 추가 안함
	if (BWAPI::Broodwar->self()->incompleteUnitCount(BWAPI::UnitTypes::Terran_Command_Center) > 0){
		return false;
	}

	//일꾼이 너무 많으면 멀티 안까도록
	if (UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_SCV) > 70){
		return false;
	}

	//아군 유닛이 적당히 나가있는 경우에만 수행한다. 이때가 비교적 안전한 경우 이므로
	if (InformationManager::Instance().nowCombatStatus >= InformationManager::combatStatus::wSecondChokePoint){
		//상대방이 멀티 숫자가 더 많은 경우(우리 멀티 숫자가 적은 경우에만 적용한다.)
		if (expansions.size() < 3 && enemyResourceRegions.size() > expansions.size()){
			std::cout << "add expansions(less than enemy expansions)" << std::endl;
			return true;
		}

		//일꾼이 남는경우
		if (WorkerManager::Instance().getNumIdleWorkers() / (float)WorkerManager::Instance().getNumMineralWorkers() > 0.5)
		{
			std::cout << "add expansions(enough workers)" << std::endl;
			return true;
		}

		// 현재 큐에 있는거 만들고도 남는 미네랄이 많다면... 빌드매니저에서 사용하는 여유자원소비기준(현재는 400)
		BuildManager &tmpObj = BuildManager::Instance();
		if ((tmpObj.getAvailableMinerals() - tmpObj.getQueueResource().first) > tmpObj.marginResource.first)
		{
			std::cout << "add expansions(enough minerals)" << std::endl;
			return true;
		}

		/*
		int minute = BWAPI::Broodwar->getFrameCount() / (24 * 60);
		int numExpansions = ExpansionManager::Instance().expansions.size();

		std::vector<int> expansionTimes = { 5, 7, 13, 20, 40, 50 };

		for (size_t i(0); i < expansionTimes.size(); ++i){
			if (numExpansions < (i + 2) && minute > expansionTimes[i]){
				std::cout << "add expansions(time limit)" << std::endl;
				return true;
			}
		}
		*/
	}

	return false;
}

void ExpansionManager::changeComplexity(BWAPI::Unit unit, bool isAdd){
	Expansion *e = getExpansion(unit);
	if (e != NULL){
		BWTA::Region *expansion_r = BWTA::getRegion(e->cc->getPosition());

		std::cout << "expansion " << e->cc->getID() << " compexity : " << e->complexity << " -> ";


		if (isAdd)
			e->complexity += (unit->getType().width() * unit->getType().height()) / expansion_r->getPolygon().getArea();
		else
			e->complexity -= (unit->getType().width() * unit->getType().height()) / expansion_r->getPolygon().getArea();

		std::cout << e->complexity << std::endl;
	}
}

Expansion * ExpansionManager::getExpansion(BWAPI::Unit u){
	Expansion *tmpRst = NULL;

	for (int i = 0; i < expansions.size(); i++){
		BWTA::Region *expansion_r = BWTA::getRegion(expansions[i].cc->getPosition());

		if (expansion_r->getPolygon().isInside(u->getPosition())){
			tmpRst = &expansions[i];
		}
	}

	return tmpRst;
}

Expansion::Expansion(){
	cc = nullptr;
	complexity = 0.0;
}

Expansion::Expansion(BWAPI::Unit u){
	cc = u;
	complexity = 0.0;
}

bool Expansion::isValid(){
	if (cc != nullptr){
		return true;
	}

	return false;
}
