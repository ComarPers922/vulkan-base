#include "Mesh.h"

void GPU_MESH::destroy()
{
    vertex_buffer.destroy();
    index_buffer.destroy();
    vertex_count = 0;
    index_count = 0;
}