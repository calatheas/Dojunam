#pragma once

#include "Common.h"
#include "MapTools.h"
#include "CommandUtil.h"

namespace MyBot
{
	class ComsatManager
	{
		std::vector<BWAPI::TilePosition> _scan_positions;
		const double _scan_dps_offset= 1.0; //min dps (a marine is 6/15)
		const int _scan_radius_offset = 224; //marine sight

		ComsatManager();

		public:
			void addScanPosition(BWAPI::TilePosition p);
			void setScanPosition();
			void update();
	};
}