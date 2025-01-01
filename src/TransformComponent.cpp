#include "TransformComponent.h"

TransformComponent::TransformComponent()
	: BaseComponent()
{

}

Matrix4x4 TransformComponent::ProduceModelTransform()
{
    auto model_transform = Matrix4x4::identity;
    model_transform[0][0] *= Transform.scale.x;
    model_transform[1][1] *= Transform.scale.y;
    model_transform[2][2] *= Transform.scale.z;

    model_transform = rotate_x(model_transform, radians(Transform.pitch));
    model_transform = rotate_y(model_transform,  radians(Transform.yaw));
    model_transform = rotate_z(model_transform,  radians(Transform.roll));

    model_transform[0][3] = Transform.position.x;
    model_transform[0][3] = Transform.position.y;
    model_transform[0][3] = Transform.position.z;

    return model_transform;
}
