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

    void SetTSS(int index, uint64_t value) {
        tss[index]      = value & 0xffffffff;
        tss[index + 1]  = value >> 32;
    }

    uint64_t AllocateStackArea(int num_4kframes) {
        auto [ stk, err ] = memory_manager->Allocate(num_4kframes);
        if (err) {
            Log(kError, "failed to allocate stack area: %s\n", err.Name());
            exit(1);
        }
        return reinterpret_cast<uint64_t>(stk.Frame()) + num_4kframes * 4096;
    }
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
    SetDataSegment(gdt[3], DescriptorType::kReadWrite, 3, 0, 0xfffff);
    SetCodeSegment(gdt[4], DescriptorType::kExecuteRead, 3, 0, 0xfffff);
    LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));
}

void InitializeSegmentation() {
    SetupSegments();

    SetDSAll(kKernelDS);           // データアクセス用のセグメントレジスタを0にしておく
    SetCSSS(kKernelCS, kKernelSS); //コード，データセグメントのセット
}

void InitializeTSS() {
    SetTSS(1, AllocateStackArea(8));                    // RSP0を確保
    SetTSS(7 + 2 * kISTForTimer, AllocateStackArea(8)); // IST1としてタイマの割り込みハンドラ実行時に自動で切り替わるためのスタック領域を確保

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