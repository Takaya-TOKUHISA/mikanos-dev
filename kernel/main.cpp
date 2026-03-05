/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル
 */
#include <cstdint>
#include <cstddef>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "font.hpp"
#define MAXVAL 255









/* 
 * <new>をインクルードすることでも実装可能
 * こちらで指定したメモリ領域のポインタを返す
 * 一般的なnewではヒープ領域を利用するmallocに近く，コンストラクタを呼び出す点が異なる．
 * newではメモリ管理機能が必要であるが，まだ実装していない段階なのでクラスインスタンスを作るため，配置newを実装する．
 * 配列を使うことで好きな大きさのメモリ領域を確保し，配置newを呼び出すことでインスタンス生成が可能になる．
 */
void* operator new(size_t size, void* buf){
    return buf;
}
/* リンク時にエラーになるので実装 */
void operator delete(void* obj) noexcept {
}

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    /* フォーマットによって利用するクラスを切り替え */
    switch (frame_buffer_config.pixel_format) {
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf)
                RGBResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf)
                BGRResv8BitPerColorPixelWriter{frame_buffer_config};
        break;
    }
    /* 白背景描画 */
    for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {
        for(int y = 0; y < frame_buffer_config.vertical_resolution; ++y) {
            pixel_writer->Write(x, y, {MAXVAL, MAXVAL, MAXVAL});
        }
    }
    /* 緑色の視覚を描画 */
    for(int x = 0; x < 200; ++x){
        for(int y = 0; y < 100; ++y) {
            pixel_writer->Write(x, y, {0, MAXVAL, 0});
        }
    }
    /* AA */
    WriteAscii(*pixel_writer, 50, 50, 'A', {0, 0, 0});
    WriteAscii(*pixel_writer, 58, 50, 'A', {0, 0, 0});

    while (1) __asm__("hlt");
}
