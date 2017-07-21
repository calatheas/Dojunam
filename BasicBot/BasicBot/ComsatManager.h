#pragma once

#include "Common.h"
#include "MapTools.h"
#include "CommandUtil.h"

namespace MyBot
{
	class ComsatManager
	{
		BWAPI::Position _scan_position;
		int _next_enable_frame;

		const double _scan_dps_offset= 1.0; //min dps (a marine is 6/15)
		const int _scan_radius_offset = 224; //marine sight

		ComsatManager();
		void setScanPosition();
		void setCommand();
		void setCommandForScout();
		void clearScanPosition();
		void setNextEnableFrame(size_t delay_frame);

		BWAPI::Position getScanPositionForScout();

		public:
			void update();
			static ComsatManager &	Instance();
	};
}
