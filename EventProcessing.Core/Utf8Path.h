#pragma once

// Qt(QString::toStdString())와 CLI 등 외부 입력은 UTF-8 std::string으로 넘어오는데,
// Windows의 std::filesystem::path(const std::string&) 생성자는 그 바이트열을 UTF-8이 아니라
// 시스템 ANSI 코드페이지로 해석한다. 경로에 한글 등 비ASCII 문자가 있으면 실제로는 존재하지
// 않는 엉뚱한 경로로 해석되어, RAW 파일을 찾지 못하거나(또는 그보다 더 나쁘게) Metavision HAL
// 내부에서 예외 처리 도중 크래시가 나는 원인이 될 수 있다. UTF-8 -> UTF-16 -> path로 변환해
// 이 문제를 피한다.

#include <filesystem>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace eventcore
{
    inline std::filesystem::path Utf8ToPath(const std::string& utf8)
    {
#ifdef _WIN32
        if (utf8.empty())
        {
            return std::filesystem::path();
        }

        const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (wlen <= 0)
        {
            return std::filesystem::path(utf8);
        }

        std::wstring wide(static_cast<size_t>(wlen), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), wlen);
        wide.resize(std::wcslen(wide.c_str()));

        return std::filesystem::path(wide);
#else
        return std::filesystem::path(utf8);
#endif
    }
}
