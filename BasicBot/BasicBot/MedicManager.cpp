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
	//int countCB = 0;
	// create a set of all medic targets
	BWAPI::Unitset medicTargets;	
	BWAPI::Unit markUnit = nullptr;
	for (auto & unit : order.getOrganicUnits())
    {		
		if (unit->getHitPoints() < unit->getType().maxHitPoints())
			// && unit->getOrderTargetPosition().getDistance(order.getPosition()) < 500 )
		//if (!unit->getType().isWorker() && !unit->getType().isMechanical() && !unit->getType().isBuilding()) //@도주남 김지훈 추가했다가 원복
        {
            medicTargets.insert(unit);
        }

		BWAPI::Unitset enemyNear;
		MapGrid::Instance().getUnitsNear(enemyNear, unit->getPosition(), 400, false, true);
		if (enemyNear.size() > 0)
			markUnit = unit;
		//@도주남 김지훈  이거 왜 이런건지 알수가 없음
		//if (unit->getHitPoints() > 0 && unit->getType().isOrganic()
		//	&& !unit->getType().isWorker()
		//	&& unit->getOrderTargetPosition().getDistance(order.getPosition()) < 500)
		//{
		//	countCB++;
		//}
    }
	//std::cout << "countCB " << countCB << std::endl;
	//std::cout << "order.getCanMedicTargets()  " << order.getCanMedicTargets() << std::endl;
	
	//std::cout << "MedicManager::executeMicro medicTargets.size(" << medicTargets.size()<< std::endl;	
    BWAPI::Unitset availableMedics(medics);

	//@도주남 김지훈 잘생각해보자 // 적 베이스에 가장 가까운 melee 유닛을 구하자
	//double closestMeleeUnitToEBDist = std::numeric_limits<double>::infinity();
	//BWAPI::Position closestMeleeP;
	

    // for each target, send the closest medic to heal it
    for (auto & target : medicTargets)
    {
		
        // only one medic can heal a target at a time
		if (target->isBeingHealed() )//&& countCB > 0)
        {
            continue;
        }

        double closestMedicDist = std::numeric_limits<double>::infinity();
        BWAPI::Unit closestMedic = nullptr;
		
        for (auto & medic : availableMedics)
        {
			//if (countCB == 0){
			//	BWAPI::Broodwar->drawTextMap(medic->getPosition() + BWAPI::Position(0, 30), "%s", "coutCB Zero In 67");
			//	BWAPI::UnitCommand currentCommand(medic->getLastCommand());
			//	Micro::SmartMove(medic, currentCommand.getTargetPosition());
			//	continue;
			//}

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
		//@도주남 김지훈 노는 메딕을 마린혹은 파벳 중심으로 보내준다.  아 안되겠다 싶으면 본진쪽 초크포인트로 돌아온다
		if (order.getOrganicUnits().size() == 0)
		{			
			BWAPI::Broodwar->drawTextMap(medic->getPosition() + BWAPI::Position(0, 30), "%s", "coutCB Zero ");
			BWAPI::UnitCommand currentCommand(medic->getLastCommand());
			Micro::SmartMove(medic, currentCommand.getTargetPosition());
		}
		else
		{
			//if (medic->getDistance(order.getPosition()) > 100 && order.getFarUnit()->getDistance(medic->getPosition()) > BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 100 && order.getFarUnit()->getID() != medic->getID())
			{	
				if (markUnit != nullptr)
				{
					BWAPI::Broodwar->drawTextMap(medic->getPosition() + BWAPI::Position(0, 30), "%s", "Marking Front ");
					Micro::SmartMove(medic, markUnit->getPosition());
				}
				else
				{
					//std::cout << "MedicManager::executeMicro 124 done " << std::endl;
					//BWAPI::UnitCommand currentCommand(medic->getLastCommand());
					if (order.getClosestUnit() != nullptr && order.getStatus() != "Move Out")
					{							
						Micro::SmartMove(medic, order.getClosestUnit()->getPosition());
						BWAPI::Broodwar->drawTextMap(medic->getPosition() + BWAPI::Position(0, 30), "%s", "\tGo ClosestUnit ");
					}
					else
					{
						Micro::SmartMove(medic, order.getPosition());						
					}
				}
				//std::cout << "MedicManager::executeMicro 121 done " << std::endl;
			}
		}
		//	Micro::SmartAttackMove(medic, InformationManager::Instance().getSecondChokePoint(InformationManager::Instance().selfPlayer)->getCenter());
		//	
    }
	
}