#include "particle.h"
#include "texture.h"
#include "debug_ostream.h"
#include <vector>
#include "direct3d.h"
#include "billboard.h"
#include "shader_particle.h"	

using namespace DirectX;

const int MAX_PARTICLES = 4000; // 最大粒子數

struct Particle {
	bool Active;            
	XMFLOAT3 Position;      // 位置
	XMFLOAT3 Velocity;      // 速度
	XMFLOAT4 Color;         // 色
	float Size;             // 大きさ
	float Age;              
	float LifeTime;         

	float Gravity;          // 重力
	float Friction;         // 阻力
	int TextureType;        
};

static std::vector<Particle> g_Particles; // 粒子池
static std::vector<int> g_TextureIDs;

void Particle_Initialize()
{
	g_Particles.resize(MAX_PARTICLES);
	for (auto& p : g_Particles) p.Active = false;
	//g_TexCirlce = Texture_Load(L"resource/texture/particle_circle.png");
	//g_TexSquare = Texture_Load(L"resource/texture/particle_square.png");
    g_TextureIDs.resize(PT_MAX);
    g_TextureIDs[PT_FIRE] = Texture_Load(L"resource/texture/fire.png");
    //g_TextureIDs[PT_SMOKE] = Texture_Load(L"resource/texture/smoke.png");
    g_TextureIDs[PT_SPARK] = Texture_Load(L"resource/texture/spark.png");
	g_TextureIDs[PT_RAIN] = Texture_Load(L"resource/texture/fire.png");
	g_TextureIDs[PT_SPLASH] = Texture_Load(L"resource/texture/fire.png");
}

void Particle_Finalize()
{
    g_Particles.clear();
    g_TextureIDs.clear();

}

void Particle_Update(double elapsed_time)
{
	for(auto& p : g_Particles)
    {
        if (!p.Active) continue;

        // --- 1. 物理運算 ---

        // 施加重力 (y軸)
        p.Velocity.y -= p.Gravity * elapsed_time;

        // 施加摩擦力/空氣阻力 (讓速度慢慢歸零)
        // Friction = 1.0 代表 1秒後速度歸零
        float frictionFactor = 1.0f - (p.Friction * elapsed_time);
        if (frictionFactor < 0.0f) frictionFactor = 0.0f; // 避免變成負數

        p.Velocity.x *= frictionFactor;
        p.Velocity.y *= frictionFactor;
        p.Velocity.z *= frictionFactor;

        // 更新位置 (p = p + v * t)
        XMVECTOR pos = XMLoadFloat3(&p.Position);
        XMVECTOR vel = XMLoadFloat3(&p.Velocity);
        pos += vel * elapsed_time;
        XMStoreFloat3(&p.Position, pos);

        // --- 2. 生命週期 ---
        p.Age += elapsed_time;

        // 淡出效果 (Alpha 隨著壽命歸零)
        // 0.0(剛出生) -> Alpha = 1.0
        // LifeTime(死前) -> Alpha = 0.0
        float lifeRatio = 1.0f - (p.Age / p.LifeTime);
        if (lifeRatio < 0.0f) lifeRatio = 0.0f;
        p.Color.w = lifeRatio;

        // 死亡判定
        if (p.Age >= p.LifeTime)
        {
            p.Active = false;
        }

		// --- 3. 特殊效果判定 ---
        if (p.TextureType == PT_RAIN)
        {
            // もし地面より下に行ったら
            if (p.Position.y <= 0.0f)
            {
                // 1. 雨粒は死ぬ
                p.Active = false;

                // 2. その場所に「水しぶき」を生成！
                // (地面スレスレに配置)
                XMFLOAT3 splashPos = { p.Position.x, 0.05f, p.Position.z };

                // 水しぶきは動かなくていいので速度0、寿命は一瞬
                Particle_Spawn(
                    splashPos,
                    { 0,0,0 },             // 速度なし
                    { 0.8f, 0.8f, 1.0f, 0.8f }, // 少し青っぽい白
                    0.1f,                // サイズ
                    0.2f,                // 寿命 (0.2秒で消える)
                    PT_SPLASH,           // 水しぶき画像
                    0.0f, 0.0f
                );
            }
        }
    }
}

void Particle_Draw(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection)
{
    ShaderParticle_Begin();

    ShaderParticle_SetWorldMatrix(XMMatrixIdentity());
    ShaderParticle_SetViewMatrix(view);
    ShaderParticle_SetProjectionMatrix(projection);

    Direct3D_SetAlphaBlendAdd();
    Direct3D_SetDepthEnable(true);
    Direct3D_SetDepthWrite(false);
    //Direct3D_SetDepthEnable(false);

    XMMATRIX invView = XMMatrixInverse(nullptr, view);
    XMFLOAT3 camRight = { XMVectorGetX(invView.r[0]), XMVectorGetY(invView.r[0]), XMVectorGetZ(invView.r[0]) };
    XMFLOAT3 camUp = { XMVectorGetX(invView.r[1]), XMVectorGetY(invView.r[1]), XMVectorGetZ(invView.r[1]) };

	static std::vector<VertexParticle> batchVertices; // バッチ用頂点配列
	batchVertices.reserve(MAX_PARTICLES * 6); // 1粒子あたり6頂点分確保

    for (int type = 0; type < PT_MAX; type++)
    {
        batchVertices.clear();
        int texID = g_TextureIDs[type];
        if (texID == -1) continue;

        for (const auto& p : g_Particles)
        {
            if (!p.Active || p.TextureType != type) continue;

            float halfSize = p.Size * 0.5f;

            float widthScale = 1.0f;
            float heightScale = 1.0f;

			if (type == PT_FIRE) // fire particles
            {
                float ratio = p.Age / p.LifeTime;


                float taperFactor = 1.0f - ratio;

                widthScale = 0.5f * taperFactor;


                heightScale = 4.0f;
            }
            else if (type == PT_SPARK)
            {
                widthScale = 0.8f;
                heightScale = 1.5f;
            }


            XMVECTOR P = XMLoadFloat3(&p.Position);


            XMVECTOR R = XMLoadFloat3(&camRight) * halfSize * widthScale;
            XMVECTOR U = XMLoadFloat3(&camUp) * halfSize * heightScale;

            XMFLOAT3 p1, p2, p3, p4;
            XMStoreFloat3(&p1, P - R + U);
            XMStoreFloat3(&p2, P + R + U);
            XMStoreFloat3(&p3, P - R - U);
            XMStoreFloat3(&p4, P + R - U);

            batchVertices.push_back({ p1, p.Color, {0.0f, 0.0f} });
            batchVertices.push_back({ p2, p.Color, {1.0f, 0.0f} });
            batchVertices.push_back({ p3, p.Color, {0.0f, 1.0f} });

            batchVertices.push_back({ p3, p.Color, {0.0f, 1.0f} });
            batchVertices.push_back({ p2, p.Color, {1.0f, 0.0f} });
            batchVertices.push_back({ p4, p.Color, {1.0f, 1.0f} });
        }

        if (!batchVertices.empty())
        {
            ShaderParticle_DrawBatch(batchVertices, texID);
        }
    }

    Direct3D_SetAlphaBlendTransparent();
    //Direct3D_SetDepthEnable(true);
}

void Particle_Spawn(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 vel, DirectX::XMFLOAT4 color, float size, float lifeTime, int type, float gravity, float friction)
{
	for (auto& p : g_Particles)
	{
		if (!p.Active)
		{
			p.Active = true;
			p.Position = pos;
			p.Velocity = vel;
			p.Color = color;
			p.Size = size;
			p.LifeTime = lifeTime;
			p.Age = 0.0f;

			p.TextureType = type;
			p.Gravity = gravity;
			p.Friction = friction;
			return;
		}
	}
}

void CreateFireEffect(XMFLOAT3 centerPos)
{
    for (int i = 0; i < 5; i++) 
    {
        float rx = (rand() % 100 - 50) * 0.01f; // -0.5 ~ 0.5
        float rz = (rand() % 100 - 50) * 0.01f;
        XMFLOAT3 pos = { centerPos.x + rx, centerPos.y, centerPos.z + rz };

        // 速度
        float vx = (rand() % 100 - 50) * 0.005f;
        float vy = (rand() % 100) * 0.01f + 0.5f;
        float vz = (rand() % 100 - 50) * 0.005f;
        XMFLOAT3 vel = { vx, vy, vz };

        XMFLOAT4 color = { 1.0f, 0.4f, 0.1f, 0.5f };

        Particle_Spawn(
            pos,
            vel,
            color,
            0.8f,    
            1.0f,    
            PT_FIRE, 
            0.0f,    
            0.1f     
        );
    }
}

void Particle_SpawnHitSparks(DirectX::XMFLOAT3 hitPos, DirectX::XMFLOAT3 hitNormal)
{
    int sparkCount = 15; 

    XMVECTOR normalVec = XMLoadFloat3(&hitNormal);

    for (int i = 0; i < sparkCount; i++)
    {
        //速度計算
        float noiseX = ((rand() % 100) - 50) * 0.01f;
        float noiseY = ((rand() % 100) - 50) * 0.01f;
        float noiseZ = ((rand() % 100) - 50) * 0.01f;
        XMVECTOR noiseVec = XMVectorSet(noiseX, noiseY, noiseZ, 0.0f);

        XMVECTOR dirVec = XMVector3Normalize(normalVec + noiseVec * 0.7f);
        // random
        float speed = 15.0f + (rand() % 100) * 0.1f;

        XMFLOAT3 vel;
        XMStoreFloat3(&vel, dirVec * speed);


        XMFLOAT4 color;
        if (rand() % 2 == 0) {
			color = { 1.0f, 0.9f, 0.5f, 1.0f }; // 黄色　内
        }
        else {
            color = { 1.0f, 0.6f, 0.2f, 1.0f }; // 赤　外
        }


        Particle_Spawn(
            hitPos, 
            vel, 
            color, 
            0.2f + (rand() % 10) * 0.01f, 
            0.3f + (rand() % 10) * 0.02f, 
            PT_SPARK, 
            15.0f,    
            3.0f      
        );
    }

}



void CreateMagicCircle(DirectX::XMFLOAT3 center)
{

    Particle_Spawn(
        center,
        { 0, 0, 0 },
        { 0.5f, 0.9f, 1.0f, 1.0f },
        4.0f,    
        0.3f, 
        PT_SPARK,
        0.0f, 0.0f
    );


    float radii[] = { 1.5f, 0.8f };
    int counts[] = { 40, 20 }; 

    for (int r = 0; r < 2; r++) 
    {
        float currentRadius = radii[r];
        int count = counts[r];

        for (int i = 0; i < count; i++)
        {
            float angle = (360.0f / count * i) * (3.14159f / 180.0f);
            float px = cos(angle) * currentRadius;
            float pz = sin(angle) * currentRadius;

            XMFLOAT3 pos = { center.x + px, center.y + 0.1f, center.z + pz };

            float tanX = -pz;
            float tanZ = px;
            float len = sqrt(tanX * tanX + tanZ * tanZ);
            tanX /= len; tanZ /= len;

            XMFLOAT3 vel = { tanX * 1.0f, 0.0f, tanZ * 1.0f };

            Particle_Spawn(
                pos,
                vel,
                { 0.1f, 0.8f, 1.0f, 1.0f }, 
                0.5f,    
                3.0f,    
                PT_SPARK,
                0.0f,
                3.0f  
            );
        }
    }


    int risingCount = 40;
    for (int i = 0; i < risingCount; i++)
    {
        float angle = (rand() % 360) * (3.14159f / 180.0f);
        float r = 0.5f + (rand() % 100) * 0.015f;

        float px = cos(angle) * r;
        float pz = sin(angle) * r;
        XMFLOAT3 pos = { center.x + px, center.y, center.z + pz };

        XMFLOAT3 vel = { -pz * 3.0f, 2.0f + (rand() % 20) * 0.1f, px * 3.0f };

        Particle_Spawn(
            pos,
            vel,
            { 0.6f, 1.0f, 1.0f, 1.0f }, 
            0.25f,
            1.5f,
            PT_SPARK,
            -1.0f, 
            0.5f
        );
    }

    // pillar
    for (int i = 0; i < 15; i++) {
        float speedY = 4.0f + (float)i * 0.5f;
        Particle_Spawn(center, { 0, speedY, 0 }, { 1,1,1,1 }, 0.6f, 1.0f, PT_SPARK, 0.0f, 0.1f);
    }
}
void CreateFirePillarVisual(DirectX::XMFLOAT3 center, float radius)
{
    int density = 3;

    float pillarHeight = 10.0f;

    for (int i = 0; i < density; i++)
    {

        float rRatio = (rand() % 100) / 100.0f;
        rRatio = sqrtf(rRatio);
        float r = rRatio * radius;

        float theta = (rand() % 360) * 3.14159f / 180.0f;
        float randomHeight = ((rand() % 100) / 100.0f) * pillarHeight;

        XMFLOAT3 pos = {
            center.x + r * cosf(theta),
            center.y + randomHeight,
            center.z + r * sinf(theta)
        };

        // (Suction Velocity)

        float dirToCenterX = -cosf(theta);
        float dirToCenterZ = -sinf(theta);


        float suctionStrength = 5.0f + (r / radius) * 15.0f;

        float speedY = 8.0f + ((rand() % 100) / 100.0f) * 5.0f; 

        XMFLOAT3 vel = {
            dirToCenterX * suctionStrength, // X 軸 內
            speedY,                         // Y 軸 上
            dirToCenterZ * suctionStrength  // Z 軸 內
        };

        // ==========================================

        XMFLOAT4 color;
        color = { 2.0f, 0.6f, 0.1f, 0.7f };

        Particle_Spawn(
            pos,
            vel,
            color,
            2.5f,  
            0.15f, 
            PT_FIRE,
            0.0f,
            0.0f
        );
    }
}

void CreateUltChargeEffect(XMFLOAT3 centerPos, float spawnRadius, int count, XMFLOAT4 color, float timeToCenter)
{
    if (timeToCenter <= 0.001f) timeToCenter = 0.1f;

    for (int i = 0; i < count; i++) 
    {
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float hOffset = ((rand() % 200) / 100.0f) - 1.0f;

        XMFLOAT3 spawnPos;
        spawnPos.x = centerPos.x + cosf(angle) * spawnRadius;
        spawnPos.y = centerPos.y + (hOffset * spawnRadius * 0.5f);
        spawnPos.z = centerPos.z + sinf(angle) * spawnRadius;

        // 1. 基礎向心速度 (往中心直飛)
        XMFLOAT3 vel;
        vel.x = (centerPos.x - spawnPos.x) / timeToCenter;
        vel.y = (centerPos.y - spawnPos.y) / timeToCenter;
        vel.z = (centerPos.z - spawnPos.z) / timeToCenter;

        // =========================================================
        // ★ 2. 加入漩渦力道 (Swirl Force)
        // 利用圓的切線方向 (-sin, cos) 讓粒子產生繞著 Y 軸旋轉的效果
        // =========================================================
        float swirlForce = 2.0f; // 數值越大，繞圈的幅度越寬、越有龍捲風的感覺
        vel.x += -sinf(angle) * swirlForce;
        vel.z += cosf(angle) * swirlForce;

        float size = 0.5f + ((rand() % 100) / 100.0f) * 1.0f;

        Particle_Spawn(
            spawnPos,
            vel,
            color,
            size,
            timeToCenter,
            PT_SPARK,
            0.0f,
            0.0f
        );
    }
}

void CreateEnergyOrbEffect(DirectX::XMFLOAT3 centerPos, DirectX::XMFLOAT4 color, float size)
{
    Particle_Spawn(
        centerPos,
        { 0.0f, 0.0f, 0.0f }, // 完全靜止
        color,
        size,               // 直接使用 Player 算好的漸大尺寸
        0.1f,               // 壽命極短，新球生出來舊球剛好消失，形成完美過渡
        PT_SPLASH,           // 使用光點貼圖
        0.0f,
        0.0f
    );
}

void CreateSlimeHurtEffect(DirectX::XMFLOAT3 centerPos)
{
    int count = 20;
    for (int i = 0; i < count; i++)
    {
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float speed = 2.0f + (rand() % 100) * 0.02f;
        XMFLOAT3 vel = {
            cosf(angle) * speed,
            1.0f + (rand() % 100) * 0.01f,
            sinf(angle) * speed
        };
        Particle_Spawn(
            centerPos,
            vel,
            { 1.0f, 1.5f, 0.5f, 1.0f }, 
            0.3f,
            1.0f,
            PT_RAIN,
            -5.0f, // 輕微上升後再落下
            1.0f   // 有一點空氣阻力
        );
    }
}
