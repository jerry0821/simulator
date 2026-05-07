// ----------------------------------------------------
// ÉpÅ[Éhì¸óÕÇÃãLò^ [pad_logger.h]
// ====================================================
// Created by: Jerry
// Date: 2025-12-03
// ----------------------------------------------------
#ifndef PAD_LOGGER_H
#define PAD_LOGGER_H

#include <windows.h>
#include <Xinput.h>
#include <DirectXMath.h>

#define BUTTON_UP       XINPUT_GAMEPAD_DPAD_UP
#define BUTTON_DOWN     XINPUT_GAMEPAD_DPAD_DOWN
#define BUTTON_LEFT     XINPUT_GAMEPAD_DPAD_LEFT
#define BUTTON_RIGHT    XINPUT_GAMEPAD_DPAD_RIGHT
#define BUTTON_START    XINPUT_GAMEPAD_START
#define BUTTON_BACK     XINPUT_GAMEPAD_BACK
#define BUTTON_L3       XINPUT_GAMEPAD_LEFT_THUMB
#define BUTTON_R3       XINPUT_GAMEPAD_RIGHT_THUMB
#define BUTTON_LB       XINPUT_GAMEPAD_LEFT_SHOULDER
#define BUTTON_RB       XINPUT_GAMEPAD_RIGHT_SHOULDER
#define BUTTON_A        XINPUT_GAMEPAD_A
#define BUTTON_B        XINPUT_GAMEPAD_B
#define BUTTON_X        XINPUT_GAMEPAD_X
#define BUTTON_Y        XINPUT_GAMEPAD_Y

void PadLogger_Initialize();
	 
void PadLogger_Update();
	 
bool PadLogger_IsPressed(DWORD user_index, WORD buttons);
bool PadLogger_IsTrigger(DWORD user_index, WORD buttons);
bool PadLogger_IsRelease(DWORD user_index, WORD buttons);

DirectX::XMFLOAT2 PadLogger_GetLeftThumbStick(DWORD user_index);
DirectX::XMFLOAT2 PadLogger_GetRightThumbStick(DWORD user_index);
float PadLogger_GetLeftTrigger(DWORD user_index);
float PadLogger_GetRightTrigger(DWORD user_index);

void PadLogger_Vibration(DWORD user_index, WORD left_motor, WORD right_motor);
// left_motor: 0-65535(ã≠êk), right_motor: 0-65535(é„êk)

void PadLogger_VibrationStop(int userIndex);


#endif // PAD_LOGGER_H