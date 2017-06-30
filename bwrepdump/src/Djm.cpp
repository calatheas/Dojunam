#include "Djm.h"

Djm::Djm(){
	std::string outFilePath = BWAPI::Broodwar->mapPathName() + ".djm";
	outFile.open(outFilePath);

	for (auto &p : BWAPI::Broodwar->getPlayers()){
		if (p->getID() > -1){
			players.push_back(p);
		}
	}
}

Djm::~Djm(){
	outFile.close();
}

void Djm::onFrame(){
	for (const auto &p : players){
		for (const auto & unit : p->getUnits()){
			outFile << BWAPI::Broodwar->getFrameCount() << ","
				<< p->getID() << ","
				<< unit->getID() << ","
				<< unit->getType() << ","
				<< unit->getPosition() << "\n";

			//outFile.flush();
		}
	}
}