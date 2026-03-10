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
#include "memory_map.hpp"
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
#include "interrupt.hpp"
#include "asmfunc.h"
#include "queue.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "window.hpp"
#include "layer.hpp"

#include "timer.hpp"

#define MAXVAL 255
#define WHITE {MAXVAL, MAXVAL, MAXVAL}
#define BLACK {0, 0, 0}


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

    StartLAPICTimer();
    console->PutString(s);
    auto elapsed = LAPICTimerElapsed();
    StopLAPCITimer();

    sprintf(s, "[%9d]", elapsed);
    console->PutString(s);
    return result;
}

char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager* memory_manager;

unsigned int mouse_layer_id;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
    layer_manager->MoveRelative(mouse_layer_id, {displacement_x, displacement_y});
    StartLAPICTimer();
    layer_manager->Draw();
    auto elapsed = LAPICTimerElapsed();
    StopLAPCITimer();
    printk("MouseObserver: elapsed = %u\n", elapsed);
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

usb::xhci::Controller* xhc;

struct Message {
    enum Type {
        kInterruptXHCI,
    } type;
};

ArrayQueue<Message>* main_queue;

__attribute__((interrupt))
void IntHandlerXHCI(InterruptFrame* frame) {
    main_queue->Push(Message{Message::kInterruptXHCI});
    NotifyEndOfInterrupt();
}

/* 新しいスタック領域として，メモリ領域 kernel_main_stack を定義 */
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

/** エントリーポイントを asm に変更するために関数名を変更
 * 引数で渡された二つのデータ構造をスタック領域のコピーしておくために二行追加
 */
extern "C" void KernelMainNewStack(const FrameBufferConfig& frame_buffer_config_ref,
                           const MemoryMap& memory_map_ref) {
    FrameBufferConfig frame_buffer_config{frame_buffer_config_ref};
    MemoryMap memory_map{memory_map_ref}; 
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

    DrawDesktop(*pixel_writer);

    console = new(console_buf) Console{
        kDesktopFGColor, kDesktopBGColor
    };
    console->SetWriter(pixel_writer);
    printk("Welcome to MikanOS!\n");
    SetLogLevel(kWarn);             // kWarn レベルに設定する(他kDebug, kInfo, (kWarn), kError)

    InitializeLAPICTimer();

    SetupSegments();

    const uint16_t kernel_cs = 1 << 3; //
    const uint16_t kernel_ss = 2 << 3; //
    SetDSAll(0);                   // データアクセス用のセグメントレジスタを0にしておく
    SetCSSS(kernel_cs, kernel_ss); //コード，データセグメントのセット

    SetupIdentityPageTable();

    ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;

    const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
    uintptr_t available_end = 0;
    for (uintptr_t iter = memory_map_base;
         iter < memory_map_base + memory_map.map_size;
         iter +=memory_map.descriptor_size) {
        auto desc = reinterpret_cast<const MemoryDescriptor*>(iter);
        if (available_end < desc->physical_start) {
            memory_manager->MarkAllocated(
                FrameID{available_end / kBytesPerFrame},
                (desc->physical_start - available_end) / kBytesPerFrame);
        }
        const auto physical_end = desc->physical_start + desc->number_of_pages * kUEFIPageSize;
        if(IsAvailable(static_cast<MemoryType>(desc->type))){
            available_end = physical_end;
        } else {
            memory_manager->MarkAllocated(
                FrameID{desc->physical_start / kBytesPerFrame},
                desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
        }
    }
    memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});

    if (auto err = InitializeHeap(*memory_manager)) {
        Log(kError, "failed to allocate pages: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
        exit(1);
    }

    std::array<Message, 32> main_queue_data;
    ArrayQueue<Message> main_queue{main_queue_data};
    ::main_queue = &main_queue;

    auto err = pci::ScanAllBus();

    /* Logは，上で設定したレベルに絞って画面に表示する */
    Log(kDebug, "ScanAllBus: %s\n", err.Name());

    for (int i = 0; i < pci::num_device; ++i){
        const auto& dev = pci::devices[i];
        auto vendor_id = pci::ReadVendorId(dev);
        auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
        Log(kDebug, "%d.%d.%d: vend %04x, class%08x, head %02x\n",
            dev.bus, dev.device, dev.function,
            vendor_id, class_code, dev.header_type);
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

    SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(IntHandlerXHCI), kernel_cs);
    /* IDT の場所を CPU に教える */
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

    /** 0xfee00020の31:24を読み取ってそのプログラムが動作しているLocal APIC ID を取得する
     * この時点では最初に起動する BSP のみしか動いていないため，BSP の Local APIC ID が取得される
     */
    const uint8_t bsp_local_apic_id = 
        *reinterpret_cast<const uint32_t*>(0xfee00020) >> 24;

    /** xHCに対してMSI割り込みを有効化する
     * 第2引数: Destination ID フィールド
     * 第5引数: Vector フィールド
     */
    pci::ConfigureMSIFixedDestination(
        *xhc_dev, bsp_local_apic_id,
        pci::MSITriggerMode::kLevel, pci::MSIDeliveryMode::kFixed,
        InterruptVector::kXHCI, 0);

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

    ::xhc = &xhc;

    usb::HIDMouseDriver::default_observer = MouseObserver;

    /* xHC群の各ポートを調べる */
    for (int i = 1; i <= xhc.MaxPorts(); ++i) {
        auto port  =xhc.PortAt(i);
        Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());
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

    const int kFrameWidth = frame_buffer_config.horizontal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;

    auto bgwindow = std::make_shared<Window>(
        kFrameWidth, kFrameHeight, frame_buffer_config.pixel_format);
    auto bgwriter = bgwindow->Writer();
    DrawDesktop(*bgwriter);
    console->SetWriter(bgwriter);

    auto mouse_window = std::make_shared<Window>(
        kMouseCursorWidth, kMouseCursorHeight, frame_buffer_config.pixel_format);
    mouse_window->SetTransparentColor(kMouseTransparentColor);
    DrawMouseCursor(mouse_window->Writer(), {0, 0});

    FrameBuffer screen;
    if (auto err = screen.Initialize(frame_buffer_config)) {
        Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
    }

    layer_manager = new LayerManager;
    layer_manager->SetWriter(&screen);
    
    auto bglayer_id = layer_manager->NewLayer()
        .SetWindow(bgwindow)
        .Move({0, 0})
        .ID();
    mouse_layer_id = layer_manager->NewLayer()
        .SetWindow(mouse_window)
        .Move({200, 200})
        .ID();

    layer_manager->UpDown(bglayer_id, 0);
    layer_manager->UpDown(mouse_layer_id, 1);
    layer_manager->Draw();

    while(true) {
        __asm__("cli");     // 割り込みフラグを0にして外部割込みを拒否する
        if (main_queue.Count() == 0) {
            __asm__("sti\n\thlt");
            continue;
        }

        Message msg = main_queue.Front();
        main_queue.Pop();
        __asm__("sti");     // pop が終わったら解放して外部割込みを許可する

        switch (msg.type) {
            case Message::kInterruptXHCI:
                while (xhc.PrimaryEventRing()->HasFront()) {
                    if (auto err = ProcessEvent(xhc)) {
                        Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
                            err.Name(), err.File(), err.Line());
                    }
                }
                break;
            default:
                Log(kError, "Unknown message type: %d\n", msg.type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}
