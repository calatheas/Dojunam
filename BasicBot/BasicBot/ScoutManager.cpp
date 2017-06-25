#include "ScoutManager.h"
#include "BuildManager.h"
#include "MapTools.h"

using namespace MyBot;

ScoutManager::ScoutManager()
	: currentScoutUnit(nullptr)
	, currentScoutStatus(ScoutStatus::NoScout)
	, currentScoutTargetBaseLocation(nullptr)
	, currentScoutTargetPosition(BWAPI::Positions::None)
	, currentScoutFreeToVertexIndex(-1)
{
}

ScoutManager & ScoutManager::Instance()
{
	static ScoutManager instance;
	return instance;
}

void ScoutManager::update()
{

	if (BWAPI::Broodwar->getFrameCount() % 6 != 0) return;


	assignScoutIfNeeded();
	moveScoutUnit();

}


void ScoutManager::assignScoutIfNeeded()
{
	//djn ssh
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());

	if (enemyBaseLocation == nullptr)
	{
		bool isScout = false;
		for (auto & unit : WorkerManager::Instance().workerData.getWorkers())
		{
			if (WorkerManager::Instance().getWorkerData().getWorkerJob(unit) == 7)
				isScout = true;
		}
		if (isScout == false)
		{
			int mineral_count_flag = 0;
			for (auto & unit : WorkerManager::Instance().workerData.getWorkers())
			{
				if (WorkerManager::Instance().getWorkerData().getWorkerJob(unit) == 0)
					mineral_count_flag++;
			}
			if (mineral_count_flag % 9 == 0)
			{
				for (auto & unit : WorkerManager::Instance().workerData.getWorkers())
				{
					if (unit->isGatheringMinerals())
						currentScoutUnit = unit;
					WorkerManager::Instance().setScoutWorker(currentScoutUnit);
					break;
				}
			}
		}
	}
	/*
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());

	if (enemyBaseLocation == nullptr)
	{
		if (!currentScoutUnit || currentScoutUnit->exists() == false || currentScoutUnit->getHitPoints() <= 0)
		{
			currentScoutUnit = nullptr;
			currentScoutStatus = ScoutStatus::NoScout;


			BWAPI::Unit firstBuilding = nullptr;

			for (auto & unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (unit->getType().isBuilding() == true && unit->getType().isResourceDepot() == false)
				{
					firstBuilding = unit;
					break;
				}
			}

			if (firstBuilding)
			{
				// grab the closest worker to the first building to send to scout
				BWAPI::Unit unit = WorkerManager::Instance().getClosestMineralWorkerTo(firstBuilding->getPosition());

				// if we find a worker (which we should) add it to the scout units
				if (unit)
				{
					// set unit as scout unit
					currentScoutUnit = unit;
					WorkerManager::Instance().setScoutWorker(currentScoutUnit);

					//WorkerManager::Instance().setIdleWorker(currentScoutUnit);
				}
			}
		}
	}
	*/
}


void ScoutManager::moveScoutUnit()
{
	if (!currentScoutUnit || currentScoutUnit->exists() == false || currentScoutUnit->getHitPoints() <= 0)
	{
		currentScoutUnit = nullptr;
		currentScoutStatus = ScoutStatus::NoScout;
		return;
	}

	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(InformationManager::Instance().enemyPlayer);
	BWTA::BaseLocation * myBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self());

	if (enemyBaseLocation == nullptr)
	{
		if (currentScoutTargetBaseLocation == nullptr || currentScoutUnit->getDistance(currentScoutTargetBaseLocation->getPosition()) < 5 * TILE_SIZE)
		{
			currentScoutStatus = ScoutStatus::MovingToAnotherBaseLocation;

			int closestDistance = INT_MAX;
			int tempDistance = 0;
			BWTA::BaseLocation * closestBaseLocation = nullptr;
			for (BWTA::BaseLocation * startLocation : BWTA::getStartLocations())
			{
				if (BWAPI::Broodwar->isExplored(startLocation->getTilePosition()) == false)
				{
					tempDistance = (int)(InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getGroundDistance(startLocation) + 0.5);

					if (tempDistance > 0 && tempDistance < closestDistance) {
						closestBaseLocation = startLocation;
						closestDistance = tempDistance;
					}
				}
			}

			if (closestBaseLocation) {
				// assign a scout to go scout it
				CommandUtil::move(currentScoutUnit, closestBaseLocation->getPosition());
				currentScoutTargetBaseLocation = closestBaseLocation;
			}
		}

	}
	// if we know where the enemy region is
	else
	{
		// if scout is exist, move scout into enemy region
		if (currentScoutUnit) {

			currentScoutTargetBaseLocation = enemyBaseLocation;

			if (BWAPI::Broodwar->isExplored(currentScoutTargetBaseLocation->getTilePosition()) == false)
			{
				currentScoutStatus = ScoutStatus::MovingToAnotherBaseLocation;

				currentScoutTargetPosition = currentScoutTargetBaseLocation->getPosition();

				CommandUtil::move(currentScoutUnit, currentScoutTargetPosition);
			}
			else {

				//currentScoutStatus = ScoutStatus::MoveAroundEnemyBaseLocation;
				//currentScoutTargetPosition = getScoutFleePositionFromEnemyRegionVertices();
				//CommandUtil::move(currentScoutUnit, myBaseLocation->getPosition());

				WorkerManager::Instance().setIdleWorker(currentScoutUnit);
				currentScoutStatus = ScoutStatus::NoScout;
				currentScoutTargetPosition = myBaseLocation->getPosition();
			}
		}
	}

}

BWAPI::Position ScoutManager::getScoutFleePositionFromEnemyRegionVertices()
{
	// calculate enemy region vertices if we haven't yet
	if (enemyBaseRegionVertices.empty()) {
		calculateEnemyRegionVertices();
	}

	if (enemyBaseRegionVertices.empty()) {
		return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	}

	// if this is the first flee, we will not have a previous perimeter index
	if (currentScoutFreeToVertexIndex == -1)
	{
		// so return the closest position in the polygon
		int closestPolygonIndex = getClosestVertexIndex(currentScoutUnit);

		if (closestPolygonIndex == -1)
		{
			return BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
		}
		else
		{
			// set the current index so we know how to iterate if we are still fleeing later
			currentScoutFreeToVertexIndex = closestPolygonIndex;
			return enemyBaseRegionVertices[closestPolygonIndex];
		}
	}
	// if we are still fleeing from the previous frame, get the next location if we are close enough
	else
	{
		double distanceFromCurrentVertex = enemyBaseRegionVertices[currentScoutFreeToVertexIndex].getDistance(currentScoutUnit->getPosition());

		// keep going to the next vertex in the perimeter until we get to one we're far enough from to issue another move command
		while (distanceFromCurrentVertex < 128)
		{
			currentScoutFreeToVertexIndex = (currentScoutFreeToVertexIndex + 1) % enemyBaseRegionVertices.size();

			distanceFromCurrentVertex = enemyBaseRegionVertices[currentScoutFreeToVertexIndex].getDistance(currentScoutUnit->getPosition());
		}

		return enemyBaseRegionVertices[currentScoutFreeToVertexIndex];
	}
}

void ScoutManager::calculateEnemyRegionVertices()
{
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
	if (!enemyBaseLocation) {
		return;
	}

	BWTA::Region * enemyRegion = enemyBaseLocation->getRegion();
	if (!enemyRegion) {
		return;
	}

	const BWAPI::Position basePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	const std::vector<BWAPI::TilePosition> & closestTobase = MapTools::Instance().getClosestTilesTo(basePosition);

	std::set<BWAPI::Position> unsortedVertices;

	// check each tile position
	for (size_t i(0); i < closestTobase.size(); ++i)
	{
		const BWAPI::TilePosition & tp = closestTobase[i];

		if (BWTA::getRegion(tp) != enemyRegion)
		{
			continue;
		}

		// a tile is 'surrounded' if
		// 1) in all 4 directions there's a tile position in the current region
		// 2) in all 4 directions there's a buildable tile
		bool surrounded = true;
		if (BWTA::getRegion(BWAPI::TilePosition(tp.x + 1, tp.y)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x + 1, tp.y))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x, tp.y + 1)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x, tp.y + 1))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x - 1, tp.y)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x - 1, tp.y))
			|| BWTA::getRegion(BWAPI::TilePosition(tp.x, tp.y - 1)) != enemyRegion || !BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(tp.x, tp.y - 1)))
		{
			surrounded = false;
		}

		// push the tiles that aren't surrounded 
		if (!surrounded && BWAPI::Broodwar->isBuildable(tp))
		{
			if (Config::Debug::DrawScoutInfo)
			{
				int x1 = tp.x * 32 + 2;
				int y1 = tp.y * 32 + 2;
				int x2 = (tp.x + 1) * 32 - 2;
				int y2 = (tp.y + 1) * 32 - 2;

				BWAPI::Broodwar->drawTextMap(x1 + 3, y1 + 2, "%d", BWTA::getGroundDistance(tp, BWAPI::Broodwar->self()->getStartLocation()));
				BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Green, false);
			}

			unsortedVertices.insert(BWAPI::Position(tp) + BWAPI::Position(16, 16));
		}
	}

	std::vector<BWAPI::Position> sortedVertices;
	BWAPI::Position current = *unsortedVertices.begin();

	enemyBaseRegionVertices.push_back(current);
	unsortedVertices.erase(current);

	// while we still have unsorted vertices left, find the closest one remaining to current
	while (!unsortedVertices.empty())
	{
		double bestDist = 1000000;
		BWAPI::Position bestPos;

		for (const BWAPI::Position & pos : unsortedVertices)
		{
			double dist = pos.getDistance(current);

			if (dist < bestDist)
			{
				bestDist = dist;
				bestPos = pos;
			}
		}

		current = bestPos;
		sortedVertices.push_back(bestPos);
		unsortedVertices.erase(bestPos);
	}

	// let's close loops on a threshold, eliminating death grooves
	int distanceThreshold = 100;

	while (true)
	{
		// find the largest index difference whose distance is less than the threshold
		int maxFarthest = 0;
		int maxFarthestStart = 0;
		int maxFarthestEnd = 0;

		// for each starting vertex
		for (int i(0); i < (int)sortedVertices.size(); ++i)
		{
			int farthest = 0;
			int farthestIndex = 0;

			// only test half way around because we'll find the other one on the way back
			for (size_t j(1); j < sortedVertices.size() / 2; ++j)
			{
				int jindex = (i + j) % sortedVertices.size();

				if (sortedVertices[i].getDistance(sortedVertices[jindex]) < distanceThreshold)
				{
					farthest = j;
					farthestIndex = jindex;
				}
			}

			if (farthest > maxFarthest)
			{
				maxFarthest = farthest;
				maxFarthestStart = i;
				maxFarthestEnd = farthestIndex;
			}
		}

		// stop when we have no long chains within the threshold
		if (maxFarthest < 4)
		{
			break;
		}

		double dist = sortedVertices[maxFarthestStart].getDistance(sortedVertices[maxFarthestEnd]);

		std::vector<BWAPI::Position> temp;

		for (size_t s(maxFarthestEnd); s != maxFarthestStart; s = (s + 1) % sortedVertices.size())
		{
			temp.push_back(sortedVertices[s]);
		}

		sortedVertices = temp;
	}

	enemyBaseRegionVertices = sortedVertices;
}

int ScoutManager::getClosestVertexIndex(BWAPI::Unit unit)
{
	int closestIndex = -1;
	double closestDistance = 10000000;

	for (size_t i(0); i < enemyBaseRegionVertices.size(); ++i)
	{
		double dist = unit->getDistance(enemyBaseRegionVertices[i]);
		if (dist < closestDistance)
		{
			closestDistance = dist;
			closestIndex = i;
		}
	}

	return closestIndex;
}
BWAPI::Unit ScoutManager::getScoutUnit()
{
	return currentScoutUnit;
}

int ScoutManager::getScoutStatus()
{
	return currentScoutStatus;
}

BWTA::BaseLocation * ScoutManager::getScoutTargetBaseLocation()
{
	return currentScoutTargetBaseLocation;
}

std::vector<BWAPI::Position> & ScoutManager::getEnemyRegionVertices()
{
	return enemyBaseRegionVertices;
}
