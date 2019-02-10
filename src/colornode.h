#pragma once

#include "FactoryNode.h"
#include "linalg.h"

class ColorNode : public FactoryNode
{
public:
	VISITABLE();

	template <typename ... Args>
	ColorNode(Args ... args) : FactoryNode(args...) { }

	Color4f color;
};
