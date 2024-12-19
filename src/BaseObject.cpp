#include "BaseObject.h"

#include "BaseComponent.h"

void BaseObject::destroy()
{
	if (components.empty())
	{
		return;
	}

	for (auto& item : components)
	{
		item->destroy();
	}

	components.clear();
}
