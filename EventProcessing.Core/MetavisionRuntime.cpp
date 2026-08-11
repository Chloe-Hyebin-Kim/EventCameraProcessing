#include "pch.h"

#ifdef EVENTCORE_HAVE_METAVISION

#include "MetavisionRuntime.h"

#include <cstdlib>
#include <filesystem>

#include <windows.h>

namespace eventcore
{
    void EnsureBundledHalPluginPath()
    {
        // getenv_s(len, nullptr, 0, name)로 필요한 버퍼 크기만 조회: len==0이면 아직 설정 안 된 것.
        // 이미 설정되어 있으면(정식 SDK 설치 등) 그 값을 존중하고 건드리지 않는다.
        size_t requiredLen = 0;
        getenv_s(&requiredLen, nullptr, 0, "MV_HAL_PLUGIN_PATH");
        if (requiredLen > 0)
        {
            return;
        }

        char exePath[MAX_PATH] = {};
        const DWORD n = ::GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        if (n == 0 || n == MAX_PATH)
        {
            return;
        }

        const std::filesystem::path pluginDir = std::filesystem::path(exePath).parent_path() / "hal_plugins";

        std::error_code ec;
        if (std::filesystem::exists(pluginDir, ec))
        {
            _putenv_s("MV_HAL_PLUGIN_PATH", pluginDir.string().c_str());
        }
    }
}

#endif // EVENTCORE_HAVE_METAVISION
