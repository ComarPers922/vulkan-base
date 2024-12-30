#pragma once
#include "BaseComponent.h"
#include "Mesh.h"

class TransformComponent: public BaseComponent
{
public:
	TransformComponent();

	Transform Transform;
	DEFAULT_DESTRUCTOR_COMPONENT(TransformComponent)
};
