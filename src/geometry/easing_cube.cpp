#include "easing_cube.h"
#include "cube.h"	
using namespace DirectX;
#include <algorithm>

void EasingCube::Update(double elapsed_time)
{
	if (!m_IsStart) return;

	m_AccumulatedTime += elapsed_time;
}

void EasingCube::Draw() const
{
	XMVECTOR start = XMLoadFloat3(&m_StartPosition);
	XMVECTOR end = XMLoadFloat3(&m_EndPosition);
	XMVECTOR v = end - start;

	double ratio = std::min(m_AccumulatedTime / m_Duration , 1.0);
	
	float ratiof = static_cast<float>(easeOutCubic(ratio));
	v *= ratiof;
	v += start;

	XMMATRIX world = XMMatrixTranslationFromVector(v);
	
	Cube_Draw(2, world); 
}