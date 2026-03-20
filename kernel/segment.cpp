#include "segment.hpp"

#include "asmfunc.h"
#include "interrupt.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"

/** GDTをSegmentDescriptorを要素として，
 * NULLディスクリプタ，カーネル用コードセグメント，カーネル用データセグメント
 * の3つを確保する
 */
namespace {
    std::array<SegmentDescriptor, 7> gdt;
    std::array<uint32_t, 26> tss; // TSSに利用するための108バイト領域

    static_assert((kTSS >> 3) + 1 < gdt.size());
}

void SetCodeSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
    desc.data = 0;

    /* セグメントの開始アドレス */
    desc.bits.base_low = base & 0xffffu;
    desc.bits.base_middle = (base >> 16) & 0xffu;
    desc.bits.base_high = (base >> 24) & 0xffu;

    /* セグメントのバイト数-1 */
    desc.bits.limit_low = limit & 0xffffu;
    desc.bits.limit_high = (limit >> 16) & 0xfu;

    desc.bits.type = type;  //ディスクリプタタイプ
    desc.bits.system_segment = 1;   // 1からコードまたはデータセグメント
    desc.bits.descriptor_privilege_level = descriptor_privilege_level; // ディスクリプタの権限レベル
    desc.bits.present = 1;  // 1なら有効なディスクリプタ
    desc.bits.available = 0; // OSが自由に使っていいビットか
    desc.bits.long_mode = 1; // 64bit用のコードセグメントか
    desc.bits.default_operation_size = 0; // long_modeが1なら0
    desc.bits.granularity = 1;      // リミットを4KiB単位で解釈する
}

void SetDataSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
    SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
    desc.bits.long_mode = 0; // データセグメントなので0
    desc.bits.default_operation_size = 1;
}

void SetSystemSegment(SegmentDescriptor& desc,
                      DescriptorType type,
                      unsigned int descriptor_privilege_level,
                      uint32_t base,
                      uint32_t limit) {
    /* セグメントディスクリプタの構造自体はCSと共通のため処理を委譲する */
    SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
    desc.bits.system_segment = 0;   // CSではなくシステムセグメント(TSS)として認識させる
    desc.bits.long_mode = 0;        // システムセグメント(TSS)なので0にする
}

void SetupSegments() {
    gdt[0].data = 0;
    /* DPL=0のコードセグメント，データセグメントの設定 */
    SetCodeSegment(gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xfffff);
    SetDataSegment(gdt[2], DescriptorType::kReadWrite, 0, 0, 0xfffff);
    /* DPL=3のコードセグメント，データセグメントの設定 */
    SetCodeSegment(gdt[3], DescriptorType::kExecuteRead, 3, 0, 0xfffff);
    SetDataSegment(gdt[4], DescriptorType::kReadWrite, 3, 0, 0xfffff);
    LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));
}

void InitializeSegmentation() {
    SetupSegments();

    SetDSAll(kKernelDS);           // データアクセス用のセグメントレジスタを0にしておく
    SetCSSS(kKernelCS, kKernelSS); //コード，データセグメントのセット
}

void InitializeTSS() {
    const int kRSP0Frames = 8; // RSP0に設定するスタック領域の大きさ
    auto [ stack0, err ] = memory_manager->Allocate(kRSP0Frames); //確保 上位4バイトは不使用で4バイト目からRSP0が始まる
    if (err) {
        Log(kError, "failed to allocate rsp0: %s\n", err.Name());
        exit(1);
    }
    /* スタック領域の末尾を計算し，rsp0に書き込む */
    uint64_t rsp0 =
        reinterpret_cast<uint64_t>(stack0.Frame()) + kRSP0Frames * 4096;
    /* tssに32ビットずつでRSP0の上位と下位のビット(スタック末尾アドレス)を記録する */
    tss[1] = rsp0 & 0xffffffff;
    tss[2] = rsp0 >> 32;

    uint64_t tss_addr = reinterpret_cast<uint64_t>(&tss[0]);
    /* GDT[5] を設定する関数 */
    SetSystemSegment(gdt[kTSS >> 3], DescriptorType::kTSSAvailable, 0,
                     tss_addr & 0xffffffff, sizeof(tss)-1);

    /** 64bit の TSS 記述子は16バイト必要であるため，GDT2エントリ分必要
     * 上で設定した次のエントリに tss_addr の上位 32bit を入れることでアドレスを完成させる
     */
    gdt[(kTSS >> 3) + 1].data = tss_addr >> 32;

    LoadTR(kTSS); // CPUに現在のタスク情報の位置をkTSSとして伝える
}