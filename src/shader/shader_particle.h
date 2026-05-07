#ifndef SHADER_PARTICLE_H
#define	SHADER_PARTICLE_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>

struct VertexParticle {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT2 uv;
};


bool ShaderParticle_Initialize();
void ShaderParticle_Finalize();
void ShaderParticle_Begin();
void ShaderParticle_DrawBatch(const std::vector<VertexParticle>& vertices, int texID);

void ShaderParticle_SetWorldMatrix(const DirectX::XMMATRIX& world);
void ShaderParticle_SetViewMatrix(const DirectX::XMMATRIX& view);
void ShaderParticle_SetProjectionMatrix(const DirectX::XMMATRIX& projection);


#endif // SHADER_PARTICLE_H