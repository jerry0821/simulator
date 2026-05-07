#pragma once
#include <DirectXMath.h>	



class EasingCube
{
private:
	double m_AccumulatedTime{};
	double m_Duration = {};
	DirectX::XMFLOAT3 m_StartPosition{};
	DirectX::XMFLOAT3 m_EndPosition{};
	bool m_IsStart = false;
private:
	double easeOutCubic(double t) const
	{
		t -= 1.0;
		return t * t * t + 1.0;
	}
public:
	EasingCube(const DirectX::XMFLOAT3& start,const DirectX::XMFLOAT3& end, double time)
		:m_StartPosition(start),m_EndPosition(end), m_Duration(time)
	{
	}

	void Start() { m_IsStart = true; }
	void Update(double elapsed_time);
	void Draw() const;
};
