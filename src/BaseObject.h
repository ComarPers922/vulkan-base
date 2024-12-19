#pragma once
#include <memory>
#include <vector>

#include "BaseComponent.h"

class BaseObject
{
public:
	std::vector<std::shared_ptr<BaseComponent>> components;

	virtual void destroy();
protected:
	BaseObject() = default;
};

