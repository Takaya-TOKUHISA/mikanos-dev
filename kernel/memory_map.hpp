#pragma once

#include <stdint.h>

struct MemoryMap {
    unsigned long long buffer_size; // マップを格納するバッファの最大サイズ
    void* buffer;                   // 実際のバッファの先頭ポインタ
    unsigned long long map_size;    // 実際のマップサイズ
    unsigned long long map_key;     // メモリマップを識別するためのカギ
    unsigned long long descriptor_size; // メモリマップ内の一つのエントリのサイズ
    uint32_t descriptor_version;        // メモリ記述子のバージョン
};

struct MemoryDescriptor {
    uint32_t type;                      // メモリ領域の用途や種類を表す
    uintptr_t physical_start;           // 物理アドレスの開始位置
    uintptr_t virtual_start;            // 仮想アドレスの開始位置
    uint64_t number_of_pages;           // メモリ領域のサイズをページ数で表したもの
    uint64_t attribute;                 // メモリの属性情報
};

#ifdef __cplusplus
enum class MemoryType {
    kEfiReservedMemoryType,
    kEfiLoaderCode,
    kEfiLoaderData,
    kEfiBootServicesCode,
    kEfiBootServicesData,
    kEfiRuntimeServicesCode,
    kEfiRuntimeServicesData,
    kEfiConventionalMemory,
    kEfiUnusableMemory,
    kEfiACPIReclaimMemory,
    kEfiACPIMemoryNVS,
    kEfiMemoryMappedIOPortSpace,
    kEfiPalCode,
    kEfiPersistentMemory,
    kEfiMaxMemoryType
};

inline bool operator==(uint32_t lhs, MemoryType rhs) {
    return lhs == static_cast<uint32_t>(rhs);
}

inline bool operator==(MemoryType lhs, uint32_t rhs) {
    return rhs == lhs;
}
#endif