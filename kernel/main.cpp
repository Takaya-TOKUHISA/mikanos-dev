/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル
 */
#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <numeric>
#include <vector>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "mouse.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

#define MAXVAL 255
#define WHITE {MAXVAL, MAXVAL, MAXVAL}
#define BLACK {0, 0, 0}


const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor = WHITE;

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

char mouse_cursor_buf[sizeof(MouseCursor)];
MouseCursor* mouse_cursor;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
    mouse_cursor->MoveRelative({displacement_x, displacement_y});
}

void SwitchEhci2Xhci(const pci::Device& xhc_dev) {
    bool intel_ehc_exist = false;
    for (int i = 0; i < pci::num_device; ++i){
        if(pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) &&
            0x8086 == pci::ReadVendorId(pci::devices[i])) {
            intel_ehc_exist = true;
            break;
        }
    }
    if(!intel_ehc_exist) {
        return;
    }

    uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev, 0xdc);
    pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports);
    uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_dev, 0xd4);
    pci::WriteConfReg(xhc_dev, 0xd0, ehci2xhci_ports);
    Log(kDebug, "SwitchEhci2Xhci: SS = %02, xHCI = %02x\n",
        superspeed_ports, ehci2xhci_ports);
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
    SetLogLevel(kWarn);             // kWarn レベルに設定する(他kDebug, kInfo, (kWarn), kError)

    mouse_cursor = new(mouse_cursor_buf) MouseCursor{
        pixel_writer, kDesktopBGColor, {300, 200}
    };
    auto err = pci::ScanAllBus();

    /* Logは，上で設定したレベルに絞って画面に表示する */
    Log(kDebug, "ScanAllBus: %s\n", err.Name());

    for (int i = 0; i < pci::num_device; ++i){
        const auto& dev = pci::devices[i];
        auto vender_id = pci::ReadVendorId(dev);
        auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
        Log(kDebug, "%d.%d.%d: vend %04x, class%08x, head %02x\n",
            dev.bus, dev.device, dev.function,
            vender_id, class_code, dev.header_type);
    }

    /* Intel 製を優先してxHCを探す */
    pci::Device* xhc_dev = nullptr;
    for (int i = 0; i < pci::num_device; ++i) {
        /** xHC の探索(0x0cu, 0x03u, x0x30u) 
         * 0x0cu : シリアルバスのコントローラ全体
         * 0x03u : USBコントローラ
         * 0x30u : xHCI
         */
        if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)) {
            xhc_dev = &pci::devices[i];
            /* みつけたものがIntelのものならばbreakして終わる */
            if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
                break;
            }
        }
    }

    if (xhc_dev) {
        Log(kInfo, "xHC has been found: %d.%d.%d\n",
            xhc_dev->bus, xhc_dev->device, xhc_dev->function);
    }

    /** xCHIの仕様ではxCHを制御するレジスタ群はMMIOである． 
     * そのため，MMIOという，メモリと同じように読み書きするメモリアドレスが付与されているレジスタを見る．
     * メモリアドレス空間のどこにあるのかは機種で異なる
     * 一方でMMIOアドレスのほうはBAR0空間に記録されているのでその値を読み取って扱う
     */
    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
    Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
    /** ~はNOT演算で，64bitなので詳しく書けば0xfは0x0000000fである
     * !(0x0000000f) = 0xfffffff0 である
     * fを複数個羅列するのは個数を数えるのが面倒なうえ，
     * やりたいことはAND演算による下位4ビットの抽出であるので
     * 以下の書き方としている
     */
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
    Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);


    /* BAR0の値を使ってxHCを初期化する */
    usb::xhci::Controller xhc{xhc_mmio_base}; //xHCIのコントローラインスタンスの生成
    /** Intel製の場合USB2.0用のEHCIと3.0用のXHCIが両方搭載されており，
     * 初期状態ではEHCIをであるので，XHCIで制御できるようにしている
     */
    if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
        SwitchEhci2Xhci(*xhc_dev);
    }
    {
        auto err = xhc.Initialize();        // 初期化
        Log(kDebug, "xhc.Initialize: %s\n", err.Name());
    }
    
    Log(kInfo, "xHC starting\n");
    xhc.Run();


    usb::HIDMouseDriver::default_observer = MouseObserver;

    /* xHC群の各ポートを調べる */
    for (int i = 1; i <= xhc.MaxPorts(); ++i) {
        auto port  =xhc.PortAt(i);
        Log(kDebug, "Port %d: IsConnected=%d\n", port.IsConnected());
        /* ポートに何らかの機器が接続されている */
        if (port.IsConnected()) {
            /* USB マウスが接続されていた場合，USB マウス用のクラスドライバに登録する */
            if (auto err = ConfigurePort(xhc, port)) {
                Log(kError, "failed to configure port: %s at %s:%d\n",
                    err.Name(), err.File(), err.Line());
                continue;
            }
        }
    }
    /** マウスが動いた際の情報がxHCにイベントという形で蓄積される
     * それを while で繰り返し読み取り，たまったイベントを処理するよう命令する．
     * "busyloop"なポーリング! 
     * 高頻度であれば効率はいいが，動いていない場合にもループし続けるのでCPUを無駄に使う
     * ループ頻度を上げると反応が悪くなる
     */
    while (1) {
        if (auto err = ProcessEvent(xhc)) {
            Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
                err.Name(), err.File(), err.Line());
        }
    }


    while (1) __asm__("hlt");
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}
