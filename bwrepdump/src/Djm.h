#pragma once

#include "Utils.h"
#include <string>

class DjmVO{
public:
	BWAPI::Race race;
	std::map<BWAPI::UnitType, int> selfUnits;
	std::map<BWAPI::UnitType, int> enemyUnits;
	std::size_t mineral;
	std::size_t gas;
	std::size_t supplyTotal;
	std::size_t supplyUsed;

	void init(std::vector<BWAPI::UnitType> &enableUnits);
	bool compare(DjmVO &vo);

	void writeVO(std::ofstream &outFile); //파일 쓰는 메소드
	void writeHeader(std::ofstream &outFile); //파일 쓰는 메소드
};

class Djm{
public:
	std::ofstream outFile;

	BWAPI::Player self;
	BWAPI::Player enemy;

	Djm();
	~Djm();

	void onFrame();
	void onUnitDestroy(BWAPI::Unit unit); //누적 유닛관리할때 삭제로직

	BWAPI::UnitType checkUnit(BWAPI::UnitType &in_type); //데이터셋으로 쓸 유닛타입 체크
	std::vector<BWAPI::UnitType> enableUnits; //데이터셋 유닛 타입저장 벡터

	bool error_replay; //테란이 없는 리플레이는 제외할때

	bool first_time; //첫 프레임 확인용
	DjmVO old_vo; //이전 프레임과 내용이 아에 동일하면 저장하지 않음

	BWAPI::Unitset enemyUnitset;
};

