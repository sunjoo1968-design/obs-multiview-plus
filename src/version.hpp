#pragma once

#ifndef MV_VERSION
#error MV_VERSION must be supplied by the build
#endif

namespace mv {
inline constexpr const char *Identity = "SunjooAn | " MV_VERSION " Controller";
inline constexpr const char *WindowTitle = "OBS Multiview Plus | " MV_VERSION " Controller | SunjooAn";
}
