#include "Djm.h"

void DjmVO::init(std::vector<BWAPI::UnitType> &enableUnits){
	bool self_finished = false;
	for (auto &u : enableUnits){
		if (u == BWAPI::UnitTypes::Terran_Physics_Lab){
			self_finished = true;
		}

		if (!self_finished) selfUnits[u] = 0;
		enemyUnits[u] = 0;
	}
}

bool DjmVO::compare(DjmVO &vo){
	if (mineral != vo.mineral) return false;
	if (gas != vo.gas) return false;
	if (gas != vo.gas) return false;
	if (supplyTotal != vo.supplyTotal) return false;
	if (supplyUsed != vo.supplyUsed) return false;

	for (auto &u : selfUnits){
		if (u.second != vo.selfUnits[u.first]) return false;
	}
	for (auto &u : enemyUnits){
		if (u.second != vo.enemyUnits[u.first]) return false;
	}

	return true;
}


Djm::Djm(){
	first_time = true;
	std::string outFilePath = BWAPI::Broodwar->mapPathName() + ".djm";
	outFile.open(outFilePath);

	self = nullptr;
	enemy = nullptr;
	for (auto &p : BWAPI::Broodwar->getPlayers()){
		if (p->getID() > -1){
			if (self == nullptr && p->getRace() == BWAPI::Races::Terran){
				self = p;
			}
			else{
				enemy = p;
			}
		}
	}

	if (self == nullptr || enemy == nullptr){
		error_replay = true;
	}
	else{
		error_replay = false;
	}

	enableUnits.push_back(BWAPI::UnitTypes::Terran_Firebat);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Ghost);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Goliath);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Marine);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Medic);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_SCV);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode); //enableUnits.push_back(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Vulture);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Battlecruiser);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Dropship);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Nuclear_Missile);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Science_Vessel);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Valkyrie);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Wraith);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Academy);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Armory);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Barracks);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Bunker);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Command_Center);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Factory);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Missile_Turret);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Refinery);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Starport);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Supply_Depot);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Comsat_Station);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Control_Tower);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Covert_Ops);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Machine_Shop);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Nuclear_Silo);
	enableUnits.push_back(BWAPI::UnitTypes::Terran_Physics_Lab); //0~31까지 테란
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Archon);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Dark_Archon);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Dark_Templar);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Dragoon);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_High_Templar);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Probe);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Reaver);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Zealot);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Arbiter);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Carrier);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Corsair);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Observer);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Scout);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Shuttle);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Assimilator);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Fleet_Beacon);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Forge);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Gateway);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Nexus);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Observatory);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Photon_Cannon);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Pylon);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Robotics_Facility);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Shield_Battery);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Stargate);
	enableUnits.push_back(BWAPI::UnitTypes::Protoss_Templar_Archives);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Defiler);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Drone);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Egg);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Hydralisk);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Infested_Terran);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Larva);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Lurker); //enableUnits.push_back(BWAPI::UnitTypes::Zerg_Lurker_Egg);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Ultralisk);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Zergling);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Devourer);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Guardian);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Mutalisk); //enableUnits.push_back(BWAPI::UnitTypes::Zerg_Cocoon);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Overlord);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Queen);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Scourge);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Creep_Colony);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Defiler_Mound);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Evolution_Chamber);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Extractor);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Greater_Spire);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Hatchery);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Hive);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Hydralisk_Den);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Infested_Command_Center);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Lair);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Nydus_Canal);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Queens_Nest);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Spawning_Pool);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Spire);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Spore_Colony);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Sunken_Colony);
	enableUnits.push_back(BWAPI::UnitTypes::Zerg_Ultralisk_Cavern);

	DjmVO vo;
	vo.init(enableUnits);
	vo.writeHeader(outFile);
}

Djm::~Djm(){
	outFile.close();
}

void Djm::onFrame(){
	if (self->leftGame() || enemy->leftGame()) return;

	DjmVO vo;
	vo.init(enableUnits);
	
	vo.race = enemy->getRace();
	vo.mineral = self->minerals();
	vo.gas = self->gas();
	vo.supplyTotal = self->supplyTotal();
	vo.supplyUsed = self->supplyUsed();

	//아군유닛은 그냥 전체 다 넣음
	BWAPI::UnitType tmpUnitType;
	for (const auto & unit : self->getUnits()){
		tmpUnitType = checkUnit(unit->getType());
		if (tmpUnitType == BWAPI::UnitTypes::None){
			continue;
		}

		vo.selfUnits[tmpUnitType]++;
	}

	//적군유닛은 누적 데이터를 넣음
	//누적데이터 생성 후 데이터셋 생성작업 함
	for (const auto & unit : enemy->getUnits()){
		//보이는 것만 함
		if (!unit->isVisible(self)){
			continue;
		}

		//기존에 있던 유닛여부
		if (enemyUnitset.find(unit) == enemyUnitset.end()){
			enemyUnitset.insert(unit);
		}
		else{

		}
	}

	for (const auto & unit : enemyUnitset){
		tmpUnitType = checkUnit(unit->getType());
		if (tmpUnitType == BWAPI::UnitTypes::None){
			continue;
		}

		vo.enemyUnits[tmpUnitType]++;
	}

	//이전 프레임하고 동일하면 쓸필요 없음
	if (first_time || !vo.compare(old_vo)){
		vo.writeVO(outFile);
	}
	if (first_time) first_time = false;
	
	old_vo = vo;
	

	/*
	for (const auto &p : players){
	for (const auto & unit : p->getUnits()){
	outFile << BWAPI::Broodwar->getFrameCount() << ","
	<< p->getID() << ","
	<< unit->getID() << ","
	<< unit->getType() << ","
	<< unit->getPosition() << "\n";

	//outFile.flush();
	}
	}
	*/
}

BWAPI::UnitType Djm::checkUnit(BWAPI::UnitType &in_type){
	if (in_type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode){
		return BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
	}
	else if (in_type == BWAPI::UnitTypes::Zerg_Cocoon){
		return BWAPI::UnitTypes::Zerg_Mutalisk;
	}
	else if (in_type == BWAPI::UnitTypes::Zerg_Lurker_Egg){
		return BWAPI::UnitTypes::Zerg_Lurker;
	}
	else{
		for (auto &i : enableUnits){
			if (in_type == i){
				return i;
			}
		}
	}

	return BWAPI::UnitTypes::None;
}

void DjmVO::writeHeader(std::ofstream &outFile){
	outFile << "Frame" << "|"
		<< "Race" << "|"
		<< "Mineral" << "|"
		<< "Gas" << "|"
		<< "SupplyUsed" << "|"
		<< "SupplyTotal";

	for (auto &u : selfUnits){
		outFile << "|" << u.first;
	}

	for (auto &u : enemyUnits){
		outFile << "|" << u.first;
	}

	outFile << "\n";
}

void DjmVO::writeVO(std::ofstream &outFile){

	outFile << BWAPI::Broodwar->getFrameCount() << "|"
		<< race << "|"
		<< mineral << "|"
		<< gas << "|"
		<< supplyUsed << "|"
		<< supplyTotal;

	for (auto &u : selfUnits){
		outFile << "|" << u.second;
	}

	for (auto &u : enemyUnits){
		outFile << "|" << u.second;
	}

	outFile << "\n";
}

void Djm::onUnitDestroy(BWAPI::Unit unit){
	enemyUnitset.erase(unit);
}