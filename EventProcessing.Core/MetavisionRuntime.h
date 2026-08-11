#pragma once

#ifdef EVENTCORE_HAVE_METAVISION

namespace eventcore
{
    // 실행 파일과 같은 폴더의 hal_plugins\ 하위 폴더(post-build 이벤트가 Prophesee\lib\metavision\hal\plugins에서
    // 복사해둔 것)가 있으면 MV_HAL_PLUGIN_PATH 환경변수를 그 경로로 설정한다.
    // 이렇게 하면 시스템에 Metavision SDK가 따로 설치/설정되어 있지 않아도(리포에 번들된 SDK만으로도)
    // Metavision::Camera가 카메라 플러그인을 찾을 수 있다.
    // MV_HAL_PLUGIN_PATH가 이미 설정되어 있으면(정식 설치 등) 건드리지 않는다.
    // Metavision::Camera를 처음 사용하기 전에 한 번 호출한다.
    void EnsureBundledHalPluginPath();
}

#endif // EVENTCORE_HAVE_METAVISION
