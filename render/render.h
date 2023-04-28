/**
 * @file      render.h
 * @brief     CUDA-accelerated rasterization pipeline.
 * @authors   Skeleton code: Yining Karl Li, Kai Ninomiya, Shuai Shao (Shrek)
 * @date      2012-2016
 * @copyright University of Pennsylvania & STUDENT
 */

#pragma once

#define TINYGLTF_LOADER_IMPLEMENTATION
#include "util/tiny_gltf_loader.h"
#include "dataType.h"

class Render {
public:
    static Render& getInstance(){
        static Render instance;
        return instance;
    }

    void init(const tinygltf::Scene & scene, const int &width, const int &height);
    void render(uchar4 *pbo, const glm::mat4 & M, const glm::mat4 & V, const glm::mat4 & P);
    void free();

private:
    Render () = default;
    ~Render () = default;
    Render (const Render &) = delete;
    Render & operator=(const Render &) = delete;

    std::map<std::string, std::vector<PrimitiveDevBufPointers>> mesh2PrimitivesMap;

    int width = 0;
    int height = 0;

    SceneInfo sceneInfo;

    Primitive *dev_primitives = nullptr;
    Fragment *dev_fragmentBuffer = nullptr;
    Tile *dev_tileBuffer = nullptr;
    glm::vec3 *dev_framebuffer = nullptr;
};


