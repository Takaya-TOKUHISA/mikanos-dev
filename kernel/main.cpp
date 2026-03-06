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
#include "pci.hpp"

#define MAXVAL 255
#define WHITE {MAXVAL, MAXVAL, MAXVAL}
#define BLACK {0, 0, 0}

/* 
 * <new>をインクルードすることでも実装可能
 * こちらで指定したメモリ領域のポインタを返す
 * 一般的なnewではヒープ領域を利用するmallocに近く，コンストラクタを呼び出す点が異なる．
 * newではメモリ管理機能が必要であるが，まだ実装していない段階なのでクラスインスタンスを作るため，配置newを実装する．
 * 配列を使うことで好きな大きさのメモリ領域を確保し，配置newを呼び出すことでインスタンス生成が可能になる．
 */
/* リンク時にエラーになるので実装 */
void operator delete(void* obj) noexcept {
}

const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor = WHITE;

/* カーソルサイズ */
const int kMouseCursorWidth = 15;
const int kMouseCursorHeight = 24;
/* カーソルのピクセル配列 */
const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
    "@              ",
    "@@             ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
};

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
    const int kFrameWidth = frame_buffer_config.horizontal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;
    
    /* 初期画面を自由に描画できる */
    /* 背景描画 */
    FillRectangle(*pixel_writer,
                  {0, 0},
                  {kFrameWidth, kFrameHeight - 50},
                  kDesktopBGColor);
    /* タスクバー的な位置にある帯 */
    FillRectangle(*pixel_writer,
                  {0, kFrameHeight - 50},
                  {kFrameWidth, 50},
                  {1, 8, 17});
    /* その左側にあるちょっと白めの帯 */
    FillRectangle(*pixel_writer,
                  {0, kFrameHeight - 50},
                  {kFrameWidth / 5, 50},
                  {80, 80, 80});
    /* その左端にある'□' */
    DrawRectangle(*pixel_writer,
                  {10, kFrameHeight - 40},
                  {30, 30},
                  {160, 160, 160});
    

    console = new(console_buf) Console{
        *pixel_writer, kDesktopFGColor, kDesktopBGColor
    };
    printk("Welcome to MikanOS!\n");

    /* カーソル描画 */
    for(int dy = 0; dy < kMouseCursorHeight; ++dy){
        for(int dx = 0; dx < kMouseCursorWidth; ++dx){
            /* 黒枠 */
            if ( mouse_cursor_shape[dy][dx] == '@') {
                pixel_writer->Write(200 + dx, 100 + dy, BLACK);
            /* 白部分 */
            }else if(mouse_cursor_shape[dy][dx] == '.') {
                pixel_writer->Write(200 + dx, 100 + dy, WHITE);
            }
        }
    }

    auto err = pci::ScanAllBus();
    printk("ScanAllBus: %s\n", err.Name());

    for (int i = 0; i < pci::num_device; ++i){
        const auto& dev = pci::devices[i];
        auto vender_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
        auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
        printk("%d.%d.%d: vend %04x, class%08x, head %02x\n",
            dev.bus, dev.device, dev.function,
            vender_id, class_code, dev.header_type);
    }
    while (1) __asm__("hlt");
}
