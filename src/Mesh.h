#pragma once

#include "lib.h"
#include "vk.h"

struct Transform
{
    Vector3 position = Vector3::ZERO;
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;
    Vector3 scale = Vector3::ZERO;
};

struct TAATransform
{
    Matrix4x4 cur;
    Matrix4x4 prev;
};

struct GPU_MESH
{
    Vk_Buffer vertex_buffer;
    Vk_Buffer index_buffer;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    void destroy();
};
