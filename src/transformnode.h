#pragma once

#include "node.h"
#include "linalg.h"
#include "FactoryNode.h"

enum transform_type_e {
	SCALE,
	ROTATE,
	MIRROR,
	TRANSLATE,
	CENTER,
	MULTMATRIX
};

class TransformNode : public FactoryNode
{
public:
	VISITABLE();

	template <typename ... Args>
	TransformNode(transform_type_e type, Args ... args) 
		: FactoryNode(args...)
		, type(type)
		, matrix(Transform3d::Identity())
	{
	}

	transform_type_e type;
	Transform3d matrix;
};
