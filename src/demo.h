#pragma once

#include "lib.h"
#include "vk.h"
#include <chrono>

struct GLFWwindow;

struct GPU_MESH
{
    Vk_Buffer vertex_buffer;
    Vk_Buffer index_buffer;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    void destroy()
	{
        vertex_buffer.destroy();
        index_buffer.destroy();
        vertex_count = 0;
        index_count = 0;
    }
};

class Vk_Demo {
public:
    void initialize(GLFWwindow* glfw_window);
    void shutdown();
    void release_resolution_dependent_resources();
    void restore_resolution_dependent_resources();
    bool vsync_enabled() const { return vsync; }
    void run_frame();

private:
    void do_imgui();
    void draw_frame();

    void color_attachment_transition_for_copy_src();
    void post_process_transition_for_copy_dst(const VkImage& targetImg);
    void simple_image_copy(const VkImage& src, const VkImage& dst, const VkExtent2D& imgExtent);
    void color_attachment_transition_for_rendering();
    void post_process_transition_for_rendering(const VkImage& targetImg);
    void color_attachment_transition_for_present();

private:
    using Clock = std::chrono::high_resolution_clock;
    using Time  = std::chrono::time_point<Clock>;

    bool show_ui = true;
    bool vsync = true;
    bool animate = false;
    int aliasingOption = true;
    float threshold = 0.1f;
    uint32_t frameIndex;

    Time last_frame_time{};
    double sim_time = 0;
    Vector3 camera_pos = Vector3(0, 0.5, 3.0);

    Vk_GPU_Time_Keeper time_keeper;
    struct {
        Vk_GPU_Time_Interval* frame;
    } gpu_times{};

    Vk_Image depth_buffer_image;
    Vk_Image post_process_image;
    Vk_Image prev_frame_image;
    Vk_Image motion_vec_image;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSetLayout post_process_descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipelineLayout post_process_pipeline_layout;
    VkPipeline pipeline;
    VkPipeline post_process_pipeline;
    Vk_Buffer post_process_descriptor_buffer;
    Vk_Buffer descriptor_buffer;
    void* post_process_mapped_descriptor_buffer_ptr = nullptr;
    void* mapped_descriptor_buffer_ptr = nullptr;
    Vk_Buffer uniform_buffer;
    void* mapped_uniform_buffer = nullptr;
    Vk_Image texture;
    VkSampler linear_sampler;
    VkSampler nearest_sampler;

    GPU_MESH gpu_mesh;
    GPU_MESH quad_mesh;
};
