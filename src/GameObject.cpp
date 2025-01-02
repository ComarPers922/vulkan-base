#include "GameObject.h"

#include "TransformComponent.h"

GameObject::GameObject()
	: BaseObject()
{
	auto newRenderable = std::make_shared<class RenderableComponent>();
	components.emplace_back(std::move(newRenderable));
	mRenderableComponent = std::dynamic_pointer_cast<class RenderableComponent>(components.back());

	auto newTransform = std::make_shared<class TransformComponent>();
	components.emplace_back(std::move(newTransform));
	mTransformComponent = std::dynamic_pointer_cast<class TransformComponent>(components.back());
}

void GameObject::DrawGameObject(VkCommandBuffer cmdBuf, VkPipelineLayout pipeline)
{
	mRenderInfo.CurrentModelMatrix = GetTransform()->ProduceModelTransform();
	auto renderable = GetRenderable();
	if (renderable)
	{
		renderable->DrawWithTextures(cmdBuf, &mRenderInfo, pipeline);
	}
	mRenderInfo.PreviousModelMatrix = mRenderInfo.CurrentModelMatrix;
}
