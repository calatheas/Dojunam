#include "ExpansionManager.h"

using namespace MyBot;

ExpansionManager & ExpansionManager::Instance()
{
	static ExpansionManager instance;
	return instance;
}
void ExpansionManager::onSendText(std::string text){

}

const std::vector<BWAPI::Unit> & ExpansionManager::getExpansions(){
	return expansions;
}

// 유닛이 파괴/사망한 경우, 해당 유닛 정보를 삭제한다
void ExpansionManager::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getPlayer() == BWAPI::Broodwar->self()){


		if (unit->getType() == BWAPI::UnitTypes::Terran_Command_Center){
			for (int i = 0; i < expansions.size(); i++){
				if (unit->getID() == expansions[i]->getID()){
					expansions.erase(expansions.begin() + i);
					WorkerManager::Instance().getWorkerData().removeDepot(unit);
					std::cout << "onUnitDestroy numExpansion:" << expansions.size() << std::endl;
					break;
				}
			}
		}
	}
}

void ExpansionManager::onUnitComplete(BWAPI::Unit unit){
	if (unit->getPlayer() == BWAPI::Broodwar->self() &&
		unit->getType() == BWAPI::UnitTypes::Terran_Command_Center){
		//numExpansion는 본진 포함개수
		for (auto &unit_in_region : unit->getUnitsInRadius(400)){
			if (unit_in_region->getType() == BWAPI::UnitTypes::Resource_Mineral_Field){
				expansions.push_back(unit);
				complexity[unit] = 0;
				std::cout << "onUnitComplete numExpansion:" << expansions.size() << std::endl;
				break;
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

			if (BuildManager::Instance().hasAddon(e)){
				comsatExists = true;
			}

			for (auto u : e->getUnitsInRadius(300)){
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
						(ConstructionManager::Instance().getConstructionQueueItemCount(BWAPI::UnitTypes::Terran_Refinery) == 0)){
						BuildManager::Instance().addBuildOrderOneItem(MetaType(BWAPI::UnitTypes::Terran_Refinery), target->getInitialTilePosition());
					}
				}
				if (!comsatExists){

					if ((BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0) &&
						!BuildManager::Instance().hasUnitInQueue(BWAPI::UnitType(BWAPI::UnitTypes::Terran_Comsat_Station))){
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
	/*
	// if there is no place to expand to, we can't expand
	if (MapTools::Instance().getNextExpansion() == BWAPI::TilePositions::None)
	{
	BWAPI::Broodwar->printf("No valid expansion location");
	return false;
	}
	*/


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

void ExpansionManager::changeComplexity(BWAPI::Unit &unit){
	for (auto &e : expansions){
		if (unit->getRegion()->getCenter() == e->getRegion()->getCenter()){
			if (complexity.find(e) == complexity.end()){
				complexity[e] = 0.0;
			}
			
			complexity[e] += (unit->getType().width() * unit->getType().height()) /
				((e->getRegion()->getBoundsRight() - e->getRegion()->getBoundsLeft())*(e->getRegion()->getBoundsBottom() - e->getRegion()->getBoundsTop()));

				/*
				std::cout << "my base-" << r->getID() << ":(" << r->getBoundsLeft() << "," << r->getBoundsTop() << "," << r->getBoundsRight() << "," << r->getBoundsBottom() << ")/"
				<< r->getCenter() << std::endl;
				break;
				}
				for (auto &u : BWAPI::Broodwar->self()->getUnits()){
				std::cout << u->getType() << ":" << u->getType().width() << "," << u->getType().height() << std::endl;
				*/
		}
	}
}