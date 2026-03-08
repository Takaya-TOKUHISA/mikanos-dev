#include "segment.hpp"

#include "asmfunc.h"

/** GDTをSegmentDescriptorを要素として，
 * NULLディスクリプタ，カーネル用コードセグメント，カーネル用データセグメント
 * の3つを確保する
 */
namespace {
    std::array<SegmentDescriptor, 3> gdt;
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

void SetupSegments() {
    gdt[0].data = 0;
    SetCodeSegment(gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xfffff);
    SetCodeSegment(gdt[2], DescriptorType::kReadWrite, 0, 0, 0xfffff);
    LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));
}