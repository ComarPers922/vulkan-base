#pragma once
#include <memory>

#include "BaseComponent.h"
#include "Mesh.h"
struct GPU_MESH;

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

    Transform Transform;

    virtual void Destroy() override;

    virtual void Draw(VkCommandBuffer cmdBuf);

    virtual void BindTextureToPipeline(VkCommandBuffer cmdBuf, VkPipelineLayout pipeline);

    virtual void DrawWithTextures(VkCommandBuffer cmdBuf, VkPipelineLayout pipeline);

    DEFAULT_DESTRUCTOR_COMPONENT(RenderableComponent)

private:
    std::unique_ptr<GPU_MESH> mMesh;
    std::unique_ptr<Vk_Image> mTexture = nullptr;
};

