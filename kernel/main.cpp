/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル
 */
#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"

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

char console_buf[sizeof(Console)];
Console* console;

int printk(const char* format, ...){
    va_list ap;
    int result;
    char s[1024];
    
    va_start(ap, format); //apをformatの次の引数にセットする
    /*
     * apを一つの引数ごとにスライドさせて受け取る．
     * 引数が過剰な場合は単に無視され，過小な場合には未定義動作となる
     * 最大1024バイトであること以外はprintfと変わらない
     */
    result = vsprintf(s, format, ap); 
    va_end(ap);

    console->PutString(s);
    return result;
}

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
    console = new(console_buf) Console{*pixel_writer, {0, 0, 0}, {MAXVAL, MAXVAL, MAXVAL}};

    for(int i = 0; i < 27; ++i){
        printk("printk: %d\n", i);
    }
    while (1) __asm__("hlt");
}
