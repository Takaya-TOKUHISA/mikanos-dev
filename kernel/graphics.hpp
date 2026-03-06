#pragma once

#include "frame_buffer_config.hpp"

struct PixelColor {
    uint8_t r, g, b;
};
/* インターフェース部分 */
class PixelWriter {
    public:
        /* コンストラクタ，config_にconfigを定数参照渡し */
        PixelWriter(const FrameBufferConfig& config) : config_{config} {
        }
        /* デストラクタ */
        virtual ~PixelWriter() = default;
        /* 純粋仮想関数:プロトタイプ宣言に近く，子クラスでの実装を強制する */
        virtual void Write(int x, int y, const PixelColor& c) = 0;

    protected:
        /* 描画ピクセルを計算する */
        uint8_t* PixelAt (int x, int y) {
            return config_.frame_buffer + 4 * (config_.pixels_per_scan_line * y + x);
        }
    
    private:
        const FrameBufferConfig& config_;
};

class RGBResv8BitPerColorPixelWriter : public PixelWriter {
    public:
        using PixelWriter::PixelWriter;
        virtual void Write(int x, int y, const PixelColor& c) override;
};

class BGRResv8BitPerColorPixelWriter : public PixelWriter {
    public:
        using PixelWriter::PixelWriter;
        virtual void Write(int x, int y, const PixelColor& c) override;
};

template <typename T>
struct Vector2D {
    T x, y;
};

void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);