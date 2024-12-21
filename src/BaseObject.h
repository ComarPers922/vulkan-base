#pragma once
#include <memory>
#include <vector>

#include "BaseComponent.h"

#define DEFAULT_DESTRUCTOR_OBJECT(CLASS_NAME) \
	virtual ~CLASS_NAME() \
	{\
		if (IsDestroyed())\
		{\
			return;\
		}\
		CLASS_NAME::Destroy();\
	}


class BaseObject
{
public:
	std::vector<std::shared_ptr<BaseComponent>> components;

	virtual void Destroy();

	const bool& IsDestroyed() const { return isDestroyed; }

	DEFAULT_DESTRUCTOR_OBJECT(BaseObject)
protected:
	BaseObject() = default;

private:
	bool isDestroyed = false;
};

