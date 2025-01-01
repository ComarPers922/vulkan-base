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

	std::shared_ptr<TransformComponent> GetTransform()
	{
		if (TransformComponent.expired())
		{
			return nullptr;
		}
		return TransformComponent.lock();
	}

	void DrawGameObject(VkCommandBuffer cmdBuf, VkPipelineLayout pipeline);

	DEFAULT_DESTRUCTOR_OBJECT(GameObject)
private:
	std::weak_ptr<RenderableComponent> RenderableComponent;
	std::weak_ptr<TransformComponent> TransformComponent;

	Matrix4x4 PreviousModelMatrix;
};
