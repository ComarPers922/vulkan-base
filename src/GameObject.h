#include "BaseObject.h"
#include "RenderableComponent.h"

class TransformComponent;

class GameObject: public BaseObject
{
public:
	GameObject();

	std::shared_ptr<RenderableComponent> GetRenderable()
	{
		if (mRenderableComponent.expired())
		{
			return nullptr;
		}
		return mRenderableComponent.lock();
	}

	std::shared_ptr<TransformComponent> GetTransform()
	{
		if (mTransformComponent.expired())
		{
			return nullptr;
		}
		return mTransformComponent.lock();
	}

	void DrawGameObject(VkCommandBuffer cmdBuf, VkPipelineLayout pipeline);

	DEFAULT_DESTRUCTOR_OBJECT(GameObject)
private:
	std::weak_ptr<RenderableComponent> mRenderableComponent;
	std::weak_ptr<TransformComponent> mTransformComponent;

	// Matrix4x4 PreviousModelMatrix;
	RenderInfo mRenderInfo;
};
