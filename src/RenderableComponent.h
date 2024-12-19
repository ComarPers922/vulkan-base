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

    Transform Transform;

    virtual void destroy() override;

    virtual void Draw(VkCommandBuffer cmdBuf);

private:
    std::unique_ptr<GPU_MESH> mMesh;
};

