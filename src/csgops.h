#pragma once

#include "node.h"
#include "enums.h"
#include "FactoryNode.h"

class CsgOpNode : public FactoryNode
{
public:
	VISITABLE();

	CsgOpNode(OpenSCADOperator type) : type(type) { }

	OpenSCADOperator type;
};
