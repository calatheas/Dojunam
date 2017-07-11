#pragma once

#include "Common.h"

namespace MyBot
{

namespace SquadOrderTypes
{
    enum { None, Idle, Attack, Defend, Regroup, Drop, SquadOrderTypes };
}

class SquadOrder
{
    size_t              _type;
    int                 _radius;
    BWAPI::Position     _position;
    std::string         _status;
	BWAPI::Unit     _farUnit;
	BWAPI::Unit     _closestUnit;
	int                 _canMedicTargets;
public:

	SquadOrder() 
		: _type(SquadOrderTypes::None)
        , _radius(0)
	{
	}

	SquadOrder(int type, BWAPI::Position position, int radius, std::string status = "Default") 
		: _type(type)
		, _position(position)
		, _radius(radius) 
		, _status(status)
	{
		_canMedicTargets = 0;
	}

	const std::string & getStatus() const 
	{
		return _status;
	}

    const BWAPI::Position & getPosition() const
    {
        return _position;
    }

    const int & getRadius() const
    {
        return _radius;
    }

    const size_t & getType() const
    {
        return _type;
    }
	
	BWAPI::Unit getFarUnit()
	{
		return _farUnit;
	}
	
	void setFarUnit(BWAPI::Unit in) 
	{
		_farUnit = in;
	}

	BWAPI::Unit getClosestUnit()
	{
		return _closestUnit;
	}

	void setClosestUnit(BWAPI::Unit in)
	{
		_closestUnit = in;
	}

	int getCanMedicTargets()
	{
		return _canMedicTargets;
	}

	void setCanMedicTargets(int in)
	{
		_canMedicTargets = in;
	}
};
}