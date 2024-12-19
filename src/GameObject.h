#include "BaseObject.h"
#include "RenderableComponent.h"

class GameObject: public BaseObject
{
public:
	GameObject();

	std::shared_ptr<RenderableComponent> GetRenderable()
	{
		return RenderableComponent.lock();
	}
private:
	std::weak_ptr<RenderableComponent> RenderableComponent;
};
