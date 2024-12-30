#include "BaseObject.h"
#include "RenderableComponent.h"

class TransformComponent;

class GameObject: public BaseObject
{
public:
	GameObject();

	std::shared_ptr<RenderableComponent> GetRenderable()
	{
		if (RenderableComponent.expired())
		{
			return nullptr;
		}
		return RenderableComponent.lock();
	}

	DEFAULT_DESTRUCTOR_OBJECT(GameObject)
private:
	std::weak_ptr<RenderableComponent> RenderableComponent;
	std::weak_ptr<TransformComponent> TransformComponent;
};
