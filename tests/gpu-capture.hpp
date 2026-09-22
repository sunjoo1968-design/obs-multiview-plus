#pragma once

#include <obs.h>
#include <graphics/vec4.h>
#include <QImage>
#include <QString>
#include <algorithm>
#include <cstring>

namespace mvtest {
// Test-only readback: bypasses desktop/GDI capture and never changes OBS output.
// Invoke on the GUI timer with a borrowed, live source (or main=true).
inline bool captureGpu(obs_source_t *source, bool main, const QString &path)
{
    constexpr uint32_t outputWidth = 640;
    constexpr uint32_t outputHeight = 360;
    if (!main && !source)
        return false;
    uint32_t width = 0, height = 0;
    if (main) {
        obs_video_info info{};
        if (!obs_get_video_info(&info))
            return false;
        width = info.base_width;
        height = info.base_height;
    } else {
        width = obs_source_get_width(source);
        height = obs_source_get_height(source);
    }
    if (!width || !height)
        return false;

    QImage image(int(outputWidth), int(outputHeight), QImage::Format_RGBA8888);
    if (image.isNull())
        return false;
    bool copied = false;
    obs_enter_graphics();
    auto *render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    auto *stage = gs_stagesurface_create(outputWidth, outputHeight, GS_RGBA);
    if (render && stage && gs_texrender_begin(render, outputWidth, outputHeight)) {
        vec4 clear{};
        clear.w = 1.0f;
        gs_clear(GS_CLEAR_COLOR, &clear, 1.0f, 0);
        const float scale = std::min(float(outputWidth) / float(width), float(outputHeight) / float(height));
        const int scaledWidth = std::max(1, int(float(width) * scale));
        const int scaledHeight = std::max(1, int(float(height) * scale));
        gs_set_viewport((int(outputWidth) - scaledWidth) / 2, (int(outputHeight) - scaledHeight) / 2,
                        scaledWidth, scaledHeight);
        gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);
        gs_blend_state_push();
        gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
        const auto culling = gs_get_cull_mode();
        gs_set_cull_mode(GS_NEITHER);
        if (main) {
            obs_render_main_texture();
        } else {
            obs_source_inc_showing(source);
            obs_source_video_render(source);
            obs_source_dec_showing(source);
        }
        gs_set_cull_mode(culling);
        gs_blend_state_pop();
        gs_texrender_end(render);
        gs_stage_texture(stage, gs_texrender_get_texture(render));
        uint8_t *pixels = nullptr;
        uint32_t stride = 0;
        if (gs_stagesurface_map(stage, &pixels, &stride)) {
            if (pixels && stride >= outputWidth * 4) {
                for (uint32_t row = 0; row < outputHeight; ++row)
                    std::memcpy(image.scanLine(int(row)), pixels + size_t(row) * stride, outputWidth * 4);
                copied = true;
            }
            gs_stagesurface_unmap(stage);
        }
    }
    gs_stagesurface_destroy(stage);
    gs_texrender_destroy(render);
    obs_leave_graphics();
    if (!copied)
        return false;

    unsigned nonblack = 0;
    for (int row = 0; row < image.height(); ++row) {
        const auto *pixels = image.constScanLine(row);
        for (int column = 0; column < image.width(); ++column) {
            const auto *pixel = pixels + column * 4;
            if (std::max({pixel[0], pixel[1], pixel[2]}) > 16)
                ++nonblack;
        }
    }
    const double ratio = double(nonblack) / double(outputWidth * outputHeight);
    const bool saved = image.save(path);
    blog(LOG_INFO, "[mv-gpu-capture] %s source=%s input=%ux%u nonblack=%.4f saved=%d",
         path.toUtf8().constData(), main ? "PROGRAM" : obs_source_get_name(source), width, height, ratio, int(saved));
    return saved && ratio > 0.01;
}
} // namespace mvtest
