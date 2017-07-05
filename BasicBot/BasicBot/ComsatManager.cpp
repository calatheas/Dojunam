#include "ComsatManager.h"

using namespace MyBot;

ComsatManager::ComsatManager()
{
	clearScanPosition();
	_next_enable_frame = -1;
}

ComsatManager & ComsatManager::Instance()
{
	static ComsatManager instance;
	return instance;
}

void ComsatManager::clearScanPosition(){
	_scan_position.x = -1;
	_scan_position.y = -1;
}

void ComsatManager::setNextEnableFrame(){
	_next_enable_frame = BWAPI::Broodwar->getFrameCount() + 24; //1초 지연
}

void ComsatManager::setScanPosition(){
	//1. 스캔 대상을 구함
	BWAPI::Unitset cloakUnits;
	UnitUtil::getAllCloakUnits(cloakUnits);

	//2. 대상 주위에 우리 유닛이 얼마나 있는지 체크
	//디텍해야되는 적 주위에 유닛이 너무 적으면(마린 3개 미만) 스캔안함
	//타겟을 공격할수 있어야 함(무기타입, 무기범위)
	std::vector<std::pair<BWAPI::Position, double>> cloakUnitInfo;
	for (auto &cu : cloakUnits){
		double tmpDps = 0.0;

		for (auto &u : cu->getUnitsInRadius(_scan_radius_offset)){	
			BWAPI::WeaponType tmpWeapon = UnitUtil::GetWeapon(u, cu); //공격가능 여부 판단 
			int tmpDistance = u->getDistance(cu); //거리 판단
			if (tmpWeapon != BWAPI::WeaponTypes::None && tmpWeapon != BWAPI::WeaponTypes::Unknown &&
				tmpDistance <= tmpWeapon.maxRange())
			{
				tmpDps += tmpWeapon.damageAmount() / (double)tmpWeapon.damageCooldown();
			}
		}

		if (tmpDps > _scan_dps_offset){
			cloakUnitInfo.push_back(std::make_pair(cu->getPosition(), tmpDps));
		}
	}

	//타겟이 없으면 리턴
	if (cloakUnitInfo.empty()) return;

	//3. 중복 스캔지역 정리
	//한 화면 안에 가장 유닛이 많은 지역을 선정 (한 화면 20타일(32*32))
	int tmpOffset = 20 * 32;
	std::vector<int> tmpInd;
	std::size_t maxCnt = 0;
	BWAPI::Position maxAreaPosition;
	for (std::size_t i = 0; i < cloakUnitInfo.size(); i++){
		for (std::size_t j = 0; j < cloakUnitInfo.size(); j++){
			tmpInd.clear();

			if (cloakUnitInfo[i].first.x <= cloakUnitInfo[j].first.x && cloakUnitInfo[i].first.x + tmpOffset > cloakUnitInfo[j].first.x &&
				cloakUnitInfo[i].first.y <= cloakUnitInfo[j].first.y && cloakUnitInfo[i].first.y + tmpOffset > cloakUnitInfo[j].first.y){
				tmpInd.push_back(j);
			}
		}

		if (tmpInd.size() > maxCnt){
			maxCnt = tmpInd.size();

			//제일 많이 겹치는 포인트들의 평균
			int tmpX = 0;
			int tmpY = 0;
			for (std::size_t k = 0; k < tmpInd.size(); k++){
				tmpX += cloakUnitInfo[k].first.x;
				tmpY += cloakUnitInfo[k].first.y;
			}
			maxAreaPosition.x = tmpX / tmpInd.size();
			maxAreaPosition.y = tmpY / tmpInd.size();
		}
	}

	//4. 가장 많은 지역의 평균좌표를 타겟으로 설정
	_scan_position.x = maxAreaPosition.x;
	_scan_position.y = maxAreaPosition.y;

}

void ComsatManager::setCommand(){
	if (_scan_position.x < 0 || _scan_position.y < 0) return;

	BWAPI::Unitset us;
	for (auto &u : BWAPI::Broodwar->self()->getUnits()){
		if (u->getType() == BWAPI::UnitTypes::Terran_Comsat_Station){
			us.insert(u);
		}
	}

	int maxEnergy = 49;
	int maxId = -1;
	for (auto &u : us){
		if (maxEnergy < u->getEnergy()){
			maxEnergy = u->getEnergy();
			maxId = u->getID();
		}
	}

	if (maxId > 0){
		BWAPI::Broodwar->getUnit(maxId)->useTech(BWAPI::TechTypes::Scanner_Sweep, _scan_position);
		setNextEnableFrame(); //1초간 스캔 안씀
	}
}

void ComsatManager::update(){
	// 1초(24프레임)에 4번 정도만 실행해도 충분하다
	if (BWAPI::Broodwar->getFrameCount() % 6 != 0) return;

	//스캔 한번 쓰면 1초 딜레이
	if (BWAPI::Broodwar->getFrameCount() > _next_enable_frame){
		clearScanPosition();
		setScanPosition();
		setCommand();
	}

	//meleeUnit->useTech(BWAPI::TechTypes::Stim_Packs);
}