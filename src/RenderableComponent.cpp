#include "RenderableComponent.h"
#include "demo.h"

RenderableComponent::RenderableComponent():
	mMesh(std::make_unique<GPU_MESH>())
	// mTexture(std::make_unique<Vk_Image>())
{

}

void RenderableComponent::Destroy()
{
	BaseComponent::Destroy();
	if (mMesh)
	{
		mMesh->destroy();
		mMesh = nullptr;
	}
	if (mTexture)
	{
		mTexture->destroy();
		mTexture = nullptr;
	}
	mRenderInfoBuffer.destroy();
}

void RenderableComponent::Draw(VkCommandBuffer cmdBuf, RenderInfo* renderInfo)
{
	const VkDeviceSize zero_offset = 0;

	vkCmdBindVertexBuffers(cmdBuf, 0, 1, &mMesh->vertex_buffer.handle, &zero_offset);
	vkCmdBindIndexBuffer(cmdBuf, mMesh->index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmdBuf, mMesh->index_count, 1, 0, 0, 0);
}

void RenderableComponent::BindTextureToPipeline(VkCommandBuffer cmdBuf, VkPipelineLayout pipeline)
{
	const auto imageInfo = VkDescriptorImageInfo{ VK_NULL_HANDLE,
	   mTexture->view,
	   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	auto write = VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.descriptorCount = 1;
	write.dstBinding = 0;
	write.pImageInfo = &imageInfo;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

	vkCmdPushDescriptorSetKHR(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline, 1, 1, &write);
}

void RenderableComponent::PushModelMatrixToPipeline(VkCommandBuffer cmdBuf, VkPipelineLayout pipeline, RenderInfo* renderInfo)
{
	memcpy(mMappedRenderInfoBuffer, renderInfo, sizeof(RenderInfo));
	const auto bufferInfo = VkDescriptorBufferInfo{
		mRenderInfoBuffer.handle,
		0,
		sizeof(RenderInfo),
	};

	auto write = VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.descriptorCount = 1;
	write.dstBinding = 1;
	write.pBufferInfo = &bufferInfo;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	vkCmdPushDescriptorSetKHR(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline, 1, 1, &write);
}

void RenderableComponent::DrawWithTextures(VkCommandBuffer cmdBuf, RenderInfo* renderInfo, VkPipelineLayout pipeline)
{
	if (renderInfo)
	{
		if (!mRenderInfoBuffer.handle)
		{
			mRenderInfoBuffer = vk_create_mapped_buffer(sizeof(RenderInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				&mMappedRenderInfoBuffer, "RenderInfo");
		}
		PushModelMatrixToPipeline(cmdBuf, pipeline, renderInfo);
	}
	BindTextureToPipeline(cmdBuf, pipeline);
	Draw(cmdBuf, renderInfo);
}
