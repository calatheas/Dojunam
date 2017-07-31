#pragma once

#include "Common.h"
//#include "Squad.h"
#include "SquadData.h"
#include "InformationManager.h"
#include "StrategyManager.h"

namespace MyBot
{
class CombatCommander
{
	SquadData       _squadData;
    BWAPI::Unitset  _combatUnits;
	BWAPI::Unitset  _dropUnits;
    bool            _initialized;
	

    void            updateScoutDefenseSquad();
	void            updateDefenseSquads();
	void            updateAttackSquads();
    void            updateDropSquads();
	void            updateIdleSquad();
	bool            isSquadUpdateFrame();
	int             getNumType(BWAPI::Unitset & units, BWAPI::UnitType type);

	BWAPI::Unit     findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender);
    BWAPI::Unit     findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target);

	BWAPI::Position getIdleSquadLastOrderLocation();
	BWAPI::Position getDefendLocation();
    BWAPI::Position getMainAttackLocation();
	BWAPI::Position getPositionForDefenceChokePoint(BWTA::Chokepoint * chokepoint);
	
	//@µµ¡÷≥≤ ±Ë¡ˆ»∆
	BWAPI::Position getMainAttackLocationForCombat(BWAPI::Position ourCenterPosition);
	bool			initMainAttackPath;
	std::vector<BWAPI::Position> mainAttackPath;
	int				curIndex;

    void            initializeSquads();
    void            verifySquadUniqueMembership();
    void            assignFlyingDefender(Squad & squad);
    void            emptySquad(Squad & squad, BWAPI::Unitset & unitsToAssign);
    int             getNumGroundDefendersInSquad(Squad & squad);
    int             getNumAirDefendersInSquad(Squad & squad);

    void            updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded);
    int             defendWithWorkers();

    int             numZerglingsInOurBase();
    bool            beingBuildingRushed();

public:
	BWAPI::Position mineralPosition;
	static CombatCommander &	Instance();
	CombatCommander();

	void update(const BWAPI::Unitset & combatUnits);
    
	void drawSquadInformation(int x, int y);
};
}
