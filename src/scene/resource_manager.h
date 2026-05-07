// ----------------------------------------------------
// 雉・ｺ千ｮ｡逅・勣 [resource_manager.h]
// ====================================================
// Created by: Jerry
// Date: 2026-03-03
// ----------------------------------------------------
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <string>

struct MODEL;

class ResourceManagerCache
{
public:
    static void initialize();
    static void finalize();

    static int getTexture(const std::wstring& filename);
    static MODEL* getModel(const std::string& filename, float scale = 1.0f);
};

namespace ResourceManager
{
    void Initialize();
    void Finalize();
    int GetTexture(const std::wstring& filename);
    MODEL* GetModel(const std::string& filename, float scale = 1.0f);
}

#endif // RESOURCE_MANAGER_H

