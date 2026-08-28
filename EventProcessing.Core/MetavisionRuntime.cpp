#include "pch.h"

#ifdef EVENTCORE_HAVE_METAVISION

#include "MetavisionRuntime.h"

#include <cstdlib>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#else
#include <climits>
#include <unistd.h>
#endif

namespace eventcore
{
    namespace
    {
        std::filesystem::path CurrentExecutablePath()
        {
#if defined(_WIN32)
            char exePath[MAX_PATH] = {};
            const DWORD n = ::GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            if (n == 0 || n == MAX_PATH)
            {
                return {};
            }
            return std::filesystem::path(exePath);
#else
            char exePath[PATH_MAX] = {};
            const ssize_t n = ::readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
            if (n <= 0)
            {
                return {};
            }
            exePath[n] = '\0';
            return std::filesystem::path(exePath);
#endif
        }

        void SetEnvVar(const char* name, const std::string& value)
        {
#if defined(_WIN32)
            _putenv_s(name, value.c_str());
#else
            ::setenv(name, value.c_str(), 1);
#endif
        }

        // Windows: Metavision HAL 플러그인은 .dll. Linux: .so
        bool IsHalPluginFile(const std::filesystem::path& p)
        {
#if defined(_WIN32)
            return p.extension() == ".dll";
#else
            return p.extension() == ".so";
#endif
        }
    }

    void EnsureBundledHalPluginPath()
    {
        const std::filesystem::path exePath = CurrentExecutablePath();
        if (exePath.empty())
        {
            return;
        }

        const std::filesystem::path pluginDir = exePath.parent_path() / "hal_plugins";

        // 실행 파일 옆에 번들된 hal_plugins\ (post-build에서 Prophesee_window\lib\metavision\hal\plugins를
        // 복사해둔 것)가 있으면, 시스템에 이미 설정된 MV_HAL_PLUGIN_PATH가 있더라도 이 리포에 맞는
        // 플러그인을 확실히 쓰도록 우선시킨다. (오래되었거나 다른 SDK 설치를 가리키는 시스템 값이
        // 남아있으면 "No plugin available" 같은 혼란스러운 에러로 이어질 수 있다.)
        std::error_code ec;
        bool hasBundledPlugin = false;
        if (std::filesystem::exists(pluginDir, ec) && std::filesystem::is_directory(pluginDir, ec))
        {
            for (const auto& entry : std::filesystem::directory_iterator(pluginDir, ec))
            {
                if (IsHalPluginFile(entry.path()))
                {
                    hasBundledPlugin = true;
                    break;
                }
            }
        }

        if (hasBundledPlugin)
        {
            SetEnvVar("MV_HAL_PLUGIN_PATH", pluginDir.string());
        }

        // 번들된 플러그인이 없으면 손대지 않는다 - 시스템에 설정된 값(있다면)이 그대로 쓰인다.
    }
}

#endif // EVENTCORE_HAVE_METAVISION
