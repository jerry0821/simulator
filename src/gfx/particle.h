#include <DirectXMath.h>


enum ParticleType {
	PT_FIRE = 0,    
	PT_SMOKE,      
	PT_SPARK,
	PT_RAIN,
	PT_SPLASH,
	PT_MAX
};


void Particle_Initialize();
void Particle_Finalize();
void Particle_Update(double elapsed_time);
void Particle_Draw(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);
void Particle_Spawn(
	DirectX::XMFLOAT3 pos,
	DirectX::XMFLOAT3 vel,
	DirectX::XMFLOAT4 color,
	float size,
	float lifeTime,
	int type,
	float gravity = 0.0f,
	float friction = 0.0f
);


void CreateFireEffect(const DirectX::XMFLOAT3 centerPos);

void CreateMagicCircle(DirectX::XMFLOAT3 center);

void Particle_SpawnHitSparks(DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal);

void CreateFirePillarVisual(DirectX::XMFLOAT3 center, float radius);

void CreateUltChargeEffect(DirectX::XMFLOAT3 centerPos, float spawnRadius, int count, DirectX::XMFLOAT4 color, float timeToCenter);
void CreateEnergyOrbEffect(DirectX::XMFLOAT3 centerPos, DirectX::XMFLOAT4 color, float baseSize);
void CreateSlimeHurtEffect(DirectX::XMFLOAT3 centerPos);