// ----------------------------------------------------
// キーボード入力の記録 [key_logger.h]
// ====================================================
// Created by: Yasuda Atsushi
// Date: 2025-06-27
// Version: 1.0
// ----------------------------------------------------

#include "key_logger.h"


static Keyboard_State g_PrevState{};
static Keyboard_State g_TriggerState{};
static Keyboard_State g_ReleaseState{};


void KeyLogger_Initialize()
{
	Keyboard_Initialize();
}

void KeyLogger_Update()
{
	const Keyboard_State* pState = Keyboard_GetState(); // 嘘ついてポインタにアドレスを入れてる
	LPBYTE pn = (LPBYTE)pState; // 今
	LPBYTE pp = (LPBYTE)&g_PrevState; // ひとつ前の結果
	LPBYTE pt = (LPBYTE)&g_TriggerState; // さっき押してなくて今押してるか
	LPBYTE pr = (LPBYTE)&g_ReleaseState; // さっき押してて今放したか

	for (int i = 0; i < sizeof(Keyboard_State); i++){
		pt[i] = (pp[i] ^ pn[i]) & pn[i];
		pr[i] = (pp[i] ^ pn[i]) & pp[i];
	}

	g_PrevState = *pState;
}

bool KeyLogger_IsPressed(Keyboard_Keys key)
{
    return Keyboard_IsKeyDown(key);
}

bool KeyLogger_IsTrigger(Keyboard_Keys key)
{
    return Keyboard_IsKeyDown(key, &g_TriggerState);
}

bool KeyLogger_IsRelease(Keyboard_Keys key)
{
	return Keyboard_IsKeyDown(key, &g_ReleaseState);
}
