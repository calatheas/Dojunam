#include "ComsatManager.h"

using namespace MyBot;

ComsatManager::ComsatManager()
{

}

void ComsatManager::setScanPosition(){
	//1. 스캔 대상을 구함
	BWAPI::Unitset cloakUnits;
	UnitUtil::getAllCloakUnits(cloakUnits);

	//2. 대상 주위에 우리 유닛이 얼마나 있는지 체크
	//디텍해야되는 적 주위에 유닛이 너무 적으면(마린 3개 미만) 스캔안함
	std::vector<std::pair<BWAPI::Position, double>> cloakUnitInfo;
	for (auto &cu : cloakUnits){
		double tmpDps = 0.0;
		for (auto &u : cu->getUnitsInRadius(_scan_radius_offset)){
			BWAPI::WeaponType tmpWeapon = UnitUtil::GetWeapon(u, cu);
			if (tmpWeapon != BWAPI::WeaponTypes::None || tmpWeapon != BWAPI::WeaponTypes::Unknown){
				tmpDps += (double)(tmpWeapon.damageAmount() / tmpWeapon.damageCooldown());
			}
		}

		if (tmpDps > _scan_dps_offset){
			cloakUnitInfo.push_back(std::make_pair(cu->getPosition(), tmpDps));
		}
	}

	//3. 중복 스캔지역 정리
	for (int i = 0; i < cloakUnitInfo.size(); i++){
		for (int j = 0; j < cloakUnitInfo.size(); j++){
			if (j == i){
				continue;
			}

		}
	}
}