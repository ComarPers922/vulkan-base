#include "GameObject.h"

GameObject::GameObject()
	: BaseObject()
{
	auto newRenderable = std::make_shared<class RenderableComponent>();
	components.emplace_back(std::move(newRenderable));
	RenderableComponent = std::dynamic_pointer_cast<class RenderableComponent>(components.back());
}
