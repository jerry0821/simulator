#ifndef SHADER_RELOAD_H
#define SHADER_RELOAD_H

#include <string>

struct ShaderReloadStatus
{
	bool succeeded = false;
	std::string message;
};

ShaderReloadStatus ShaderReload_ReloadAll();

#endif // SHADER_RELOAD_H
