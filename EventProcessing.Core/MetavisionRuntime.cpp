#include "pch.h"
#include "MetavisionRuntime.h"

#include <cstdlib>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace eventcore
{
    void EnsureBundledHalPluginPath()
    {
#ifdef EVENTCORE_HAVE_METAVISION
        std::filesystem::path executable;
#if defined(_WIN32)
        wchar_t path[MAX_PATH] = {};
        const DWORD size = ::GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (size == 0 || size == MAX_PATH) return;
        executable = path;
#elif defined(__linux__)
        char path[PATH_MAX] = {};
        const ssize_t size = ::readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (size <= 0) return;
        path[size] = '\0';
        executable = path;
#else
        return;
#endif
        const std::filesystem::path pluginDirectory = executable.parent_path() / "hal_plugins";
        std::error_code error;
        if (!std::filesystem::is_directory(pluginDirectory, error)) return;

        bool foundPlugin = false;
        for (const auto& entry : std::filesystem::directory_iterator(pluginDirectory, error)) {
#if defined(_WIN32)
            foundPlugin = entry.path().extension() == ".dll";
#else
            foundPlugin = entry.path().extension() == ".so";
#endif
            if (foundPlugin) break;
        }
        if (!foundPlugin) return;
#if defined(_WIN32)
        _wputenv_s(L"MV_HAL_PLUGIN_PATH", pluginDirectory.c_str());
#else
        ::setenv("MV_HAL_PLUGIN_PATH", pluginDirectory.c_str(), 1);
#endif
#endif
    }
}
