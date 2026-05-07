// ----------------------------------------------------
// 雉・ｺ千ｮ｡逅・勣 [resource_manager.cpp]
// ====================================================
// Created by: Jerry
// Date: 2026-03-03
// ----------------------------------------------------
#include "resource_manager.h"
#include "texture.h"
#include "model.h"
#include <unordered_map>

static std::unordered_map<std::wstring, int> g_TextureCache;
static std::unordered_map<std::string, MODEL*> g_ModelCache;

namespace ResourceManager
{
}

void ResourceManagerCache::initialize()
{
    g_TextureCache.clear();
    g_ModelCache.clear();
}

void ResourceManagerCache::finalize()
{
    g_TextureCache.clear();
    g_ModelCache.clear();
}

int ResourceManagerCache::getTexture(const std::wstring& filename)
{
    auto it = g_TextureCache.find(filename);
    if (it != g_TextureCache.end()) {
        return it->second;
    }

    const int texture_id = TextureManager::Load(filename.c_str());
    g_TextureCache[filename] = texture_id;
    return texture_id;
}

MODEL* ResourceManagerCache::getModel(const std::string& filename, float scale)
{
    const std::string key = filename + "_" + std::to_string(scale);
    auto it = g_ModelCache.find(key);
    if (it != g_ModelCache.end()) {
        return it->second;
    }

    MODEL* model = ModelLoad(filename.c_str(), scale);
    g_ModelCache[key] = model;
    return model;
}

namespace ResourceManager
{
    void Initialize()
    {
        ResourceManagerCache::initialize();
    }

    void Finalize()
    {
        ResourceManagerCache::finalize();
    }

    int GetTexture(const std::wstring& filename)
    {
        return ResourceManagerCache::getTexture(filename);
    }

    MODEL* GetModel(const std::string& filename, float scale)
    {
        return ResourceManagerCache::getModel(filename, scale);
    }
}

