#include "GameObject.h"

#include "TransformComponent.h"

GameObject::GameObject()
	: BaseObject()
{
	auto newRenderable = std::make_shared<class RenderableComponent>();
	components.emplace_back(std::move(newRenderable));
	RenderableComponent = std::dynamic_pointer_cast<class RenderableComponent>(components.back());

	auto newTransform = std::make_shared<class TransformComponent>();
	components.emplace_back(std::move(newTransform));
	TransformComponent = std::dynamic_pointer_cast<class TransformComponent>(components.back());
}
