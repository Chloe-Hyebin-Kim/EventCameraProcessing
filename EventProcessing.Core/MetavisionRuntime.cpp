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
        char exePath[MAX_PATH] = {};
        const DWORD n = ::GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        if (n == 0 || n == MAX_PATH)
        {
            return;
        }

        const std::filesystem::path pluginDir = std::filesystem::path(exePath).parent_path() / "hal_plugins";

        // 실행 파일 옆에 번들된 hal_plugins\ (post-build에서 Prophesee\lib\metavision\hal\plugins를
        // 복사해둔 것)가 있으면, 시스템에 이미 설정된 MV_HAL_PLUGIN_PATH가 있더라도 이 리포에 맞는
        // 플러그인을 확실히 쓰도록 우선시킨다. (오래되었거나 다른 SDK 설치를 가리키는 시스템 값이
        // 남아있으면 "No plugin available" 같은 혼란스러운 에러로 이어질 수 있다.)
        std::error_code ec;
        bool hasBundledPlugin = false;
        if (std::filesystem::exists(pluginDir, ec) && std::filesystem::is_directory(pluginDir, ec))
        {
            for (const auto& entry : std::filesystem::directory_iterator(pluginDir, ec))
            {
                if (entry.path().extension() == ".dll")
                {
                    hasBundledPlugin = true;
                    break;
                }
            }
        }

        if (hasBundledPlugin)
        {
            _putenv_s("MV_HAL_PLUGIN_PATH", pluginDir.string().c_str());
        }

        // 번들된 플러그인이 없으면 손대지 않는다 - 시스템에 설정된 값(있다면)이 그대로 쓰인다.
    }
}

#endif // EVENTCORE_HAVE_METAVISION
