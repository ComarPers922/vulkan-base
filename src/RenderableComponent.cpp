#include "RenderableComponent.h"
#include "demo.h"

RenderableComponent::RenderableComponent():
	mMesh(std::make_unique<GPU_MESH>()),
	mTexture(std::make_unique<Vk_Image>())
{
	
}

void RenderableComponent::Destroy()
{
	BaseComponent::Destroy();
	mMesh->destroy();
}

void RenderableComponent::Draw(VkCommandBuffer cmdBuf)
{
	const VkDeviceSize zero_offset = 0;

	vkCmdBindVertexBuffers(cmdBuf, 0, 1, &mMesh->vertex_buffer.handle, &zero_offset);
	vkCmdBindIndexBuffer(cmdBuf, mMesh->index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmdBuf, mMesh->index_count, 1, 0, 0, 0);
}
