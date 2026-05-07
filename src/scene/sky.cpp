// ----------------------------------------------------
// 空の表示 [sky.cpp]
// ====================================================
// Created by: Jerry
// Date: 2025-11-21
// ----------------------------------------------------

#include "sky.h"

#include "direct3d.h"
#include "model.h"
#include "render_state.h"
#include "shader3d_unlit.h"
#include "texture.h"

using namespace DirectX;

namespace
{
MODEL* g_sky_model = nullptr;
XMFLOAT3 g_sky_position{};
constexpr float kSkyFixedY = 0.0f;
}

void Sky_Initialize()
{
    g_sky_model = ModelLoad("resource/model/sky.fbx", 200.0f, true);
}

void Sky_Finalize()
{
    if (g_sky_model != nullptr)
    {
        ModelRelease(g_sky_model);
        g_sky_model = nullptr;
    }
}

void Sky_SetPosition(const DirectX::XMFLOAT3& position)
{
    g_sky_position = position;
}

void Sky_Draw()
{
    if (g_sky_model == nullptr)
    {
        return;
    }

    // Keep the sky centered on the camera horizontally while fixing its height.
    XMFLOAT3 draw_position = g_sky_position;
    draw_position.y = kSkyFixedY;

    const XMMATRIX world_matrix =
        XMMatrixTranslationFromVector(XMLoadFloat3(&draw_position));

    ModelUnlitDraw(
        g_sky_model,
        world_matrix,
        RenderState{ DepthMode::Disabled, BlendMode::Opaque, CullMode::None });
}
