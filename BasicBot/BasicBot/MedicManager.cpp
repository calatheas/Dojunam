#include "MedicManager.h"
#include "CommandUtil.h"

using namespace MyBot;

MedicManager::MedicManager() 
{ 
}

void MedicManager::executeMicro(const BWAPI::Unitset & targets) 
{
	const BWAPI::Unitset & medics = getUnits();

	//@도주남 김지훈 파벳 마린의 수를 구한다.
	int countCB = 0;
	// create a set of all medic targets
	BWAPI::Unitset medicTargets;
	double minDistance = std::numeric_limits<double>::max();
	BWAPI::Position meleeUnitsetCenterP = order.getPosition();
    for (auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (unit->getHitPoints() < unit->getInitialHitPoints() && !unit->getType().isMechanical() && !unit->getType().isBuilding())
		//if (!unit->getType().isWorker() && !unit->getType().isMechanical() && !unit->getType().isBuilding()) //@도주남 김지훈 추가했다가 원복
        {
            medicTargets.insert(unit);
        }
		if (unit->getHitPoints() > 0 && unit->getType() == InformationManager::Instance().getBasicCombatUnitType() && !unit->getType().isMechanical() && !unit->getType().isBuilding())
		{
			if (unit->getDistance(order.getPosition()) < minDistance)
			{
				meleeUnitsetCenterP = unit->getPosition();
				minDistance = unit->getDistance(order.getPosition());
			}
			countCB++;
		}
    }
	//std::cout << "MedicManager::executeMicro medicTargets.size(" << medicTargets.size()<< std::endl;
    
    BWAPI::Unitset availableMedics(medics);

	//@도주남 김지훈 잘생각해보자 // 적 베이스에 가장 가까운 melee 유닛을 구하자
	//double closestMeleeUnitToEBDist = std::numeric_limits<double>::infinity();
	//BWAPI::Position closestMeleeP;
	

    // for each target, send the closest medic to heal it
    for (auto & target : medicTargets)
    {
		//closestMeleeP = target->getPosition();
		//_mainBaseLocations[selfPlayer]
		//int gDist = target->getDistance(InformationManager::Instance().getMainBaseLocation(InformationManager::Instance().enemyPlayer)->getPosition());
		//
		//	if (gDist < closestMeleeUnitToEBDist && target->getType() !=BWAPI::UnitTypes::Terran_Medic)
		//{
		//	std::cout << "in 414141 [" << gDist << std::endl;
		//	closestMeleeP = target->getPosition();
		//	std::cout << "in x [" << closestMeleeP.x << "] y[ " << closestMeleeP.y << std::endl;
		//	closestMeleeUnitToEBDist = gDist;
		//}

        // only one medic can heal a target at a time
        if (target->isBeingHealed())
        {
            continue;
        }

        double closestMedicDist = std::numeric_limits<double>::infinity();
        BWAPI::Unit closestMedic = nullptr;

        for (auto & medic : availableMedics)
        {
            double dist = medic->getDistance(target);

            if (!closestMedic || (dist < closestMedicDist))
            {
                closestMedic = medic;
                closestMedicDist = dist;
            }
        }

        // if we found a medic, send it to heal the target
        if (closestMedic)
        {
            closestMedic->useTech(BWAPI::TechTypes::Healing, target);

            availableMedics.erase(closestMedic);
        }
        // otherwise we didn't find a medic which means they're all in use so break
        else
        {
            break;
        }
    }

    // the remaining medics should head to the squad order position
    for (auto & medic : availableMedics)
    {
		//if (closestMeleeUnitToEBDist < 
		//	MapTools::Instance().getGroundDistance(medic->getPosition(), InformationManager::Instance().getMainBaseLocation(InformationManager::Instance().enemyPlayer)->getPosition()) )
		//{
		//	std::cout << "closestMeleeUnitToEBDist " << closestMeleeUnitToEBDist << std::endl;
		//	Micro::SmartAttackMove(medic, closestMeleeP);
		//}
		//else
		//@도주남 김지훈 노는 메딕을 마린혹은 파벳 중심으로 보내준다.  아 안되겠다 싶으면 본진쪽 초크포인트로 돌아온다
		if (countCB > medics.size())
			Micro::SmartAttackMove(medic, meleeUnitsetCenterP);
		else
		{
			BWAPI::UnitCommand currentCommand(medic->getLastCommand());
			Micro::SmartAttackMove(medic, currentCommand.getTargetPosition());
		}

		//	Micro::SmartAttackMove(medic, InformationManager::Instance().getSecondChokePoint(InformationManager::Instance().selfPlayer)->getCenter());
		//
		
    }
}