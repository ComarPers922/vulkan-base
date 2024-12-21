#include "BaseObject.h"

#include "BaseComponent.h"

void BaseObject::Destroy()
{
	if (components.empty())
	{
		return;
	}

	for (auto& item : components)
	{
		item->Destroy();
	}

	components.clear();

	isDestroyed = true;
}

