// ----------------------------------------------------
// [main.cpp]
// ====================================================
// Created by: 
// Date: 2025-06-18
// Version: 1.0
// ----------------------------------------------------
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "application.h"

// 
int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR,
                     _In_ int nCmdShow) {
  Application application;
  if (!application.Initialize(hInstance, nCmdShow)) {
    return -1;
  }

  const int result = application.Run();
  application.Shutdown();
  return result;
}

