#pragma once

#include "Utils.h"

class Djm{
public:
	std::ofstream outFile;

	std::list<BWAPI::Player> players;

	Djm();
	~Djm();

	void onFrame();
};