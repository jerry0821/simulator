#ifndef RUNTIME_SYSTEM_H
#define RUNTIME_SYSTEM_H

#include <Windows.h>

namespace RuntimeSystem
{
bool Initialize(HWND window_handle);
void Finalize();
}

#endif // RUNTIME_SYSTEM_H
