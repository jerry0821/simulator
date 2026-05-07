







#ifndef TEXTURE_H
#define TEXTURE_H

#include <d3d11.h>

class TextureManager
{
public:
	static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
	static void Finalize();

	static int Load(const wchar_t* filename);
	static void ReleaseAll();

	static void SetTexture(int texture_id, int slot = 0);
	static void SetExternalSRV(ID3D11ShaderResourceView* srv, int slot = 0);
	static unsigned int Width(int texture_id);
	static unsigned int Height(int texture_id);

	static ID3D11ShaderResourceView* GetSRV(int texture_id);
};

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Texture_Finalize(void);

// テクスチャ画像の読み込み
//
// 戻り値　: 管理番号。読み込めなかった場合-1。
//
int Texture_Load(const wchar_t* pFiliname);

// テクスチャの開放
void Texture_AllRelease();

void Texture_SetTexture(int texid, int slot = 0);
void Texture_SetExternalSRV(ID3D11ShaderResourceView* srv, int slot = 0);
unsigned int Texture_Width(int texid);
unsigned int Texture_Height(int texid);


ID3D11ShaderResourceView* Texture_GetSRV(int texture_id);

#endif // TEXTURE_H

