#pragma once

#ifndef MV_VERSION
#error MV_VERSION must be supplied by the build
#endif

namespace mv {
inline constexpr const char *Identity = " | " MV_VERSION " | OBS Link Multiview";
inline constexpr const char *WindowTitle = "OBS Link Multiview | " MV_VERSION " | ";
}
