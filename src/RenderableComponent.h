#pragma once
#include <memory>

#include "BaseComponent.h"
#include "Mesh.h"
struct GPU_MESH;

struct RenderInfo
{
    Matrix4x4 CurrentModelMatrix;
    Matrix4x4 PreviousModelMatrix;
};

class RenderableComponent :
    public BaseComponent
{
public:
    RenderableComponent();

    std::unique_ptr<GPU_MESH>& GetGPUMesh()
    {
        return mMesh;
    }

    std::unique_ptr<Vk_Image>& GetTexture()
    {
        return mTexture;
    }

    virtual void Destroy() override;

    virtual void Draw(VkCommandBuffer cmdBuf, RenderInfo* renderInfo);

    virtual void BindTextureToPipeline(VkCommandBuffer cmdBuf, VkPipelineLayout pipeline);

    virtual void DrawWithTextures(VkCommandBuffer cmdBuf, RenderInfo* renderInfo, VkPipelineLayout pipeline);

    DEFAULT_DESTRUCTOR_COMPONENT(RenderableComponent)

protected:
    virtual void PushModelMatrixToPipeline(VkCommandBuffer cmdBuf, VkPipelineLayout pipeline, RenderInfo* renderInfo);

private:
    std::unique_ptr<GPU_MESH> mMesh;
    std::unique_ptr<Vk_Image> mTexture = nullptr;

    Vk_Buffer mRenderInfoBuffer;
    void* mMappedRenderInfoBuffer;
};

