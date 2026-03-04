#pragma once

#include <stdint.h>

/* 今回はPixelBitMaskとPixelBltOnlyの実装は省き，以下の2種類についてのみ対応する */
enum PixelFormat {
    kPixelRGBResv8BitPerColor,
    kPixelBGRResv8BitPerColor,
};

struct FrameBufferConfig {
    uint8_t* frame_buffer;
    uint32_t pixels_per_scan_line;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    enum PixelFormat pixel_format;
};