#include <Uefi.h>                               // UEFIの基本的な型や定数を定義(EFI_STATUSなど)
#include <Library/UefiLib.h>                    // UEFIのライブラリ関数を定義(Printなど)
#include <Library/UefiBootServicesTableLib.h>   // UEFIのブートサービステーブルを定義(gBSなど)
#include <Library/PrintLib.h>                   // 高度なPrint関数を定義
#include <Library/MemoryAllocationLib.h>        // mallocに近い感覚で必要なメモリを確保する
#include <Library/BaseMemoryLib.h>              // string.hの代わり
#include <Protocol/LoadedImage.h>               // ロードされたイメージに関するプロトコルを定義
#include <Protocol/SimpleFileSystem.h>          // ファイル操作の機能提供
#include <Protocol/DiskIo2.h>                   // 非同期なオフセット指定での読み書きを提供
#include <Protocol/BlockIo.h>                   // ブロック単位での読み書きを提供(512B等)
#include <Guid/FileInfo.h>                      // ファイルのメタデータの取得
#include "frame_buffer_config.hpp"
#include "memory_map.hpp"
#include "elf.hpp"

/* メモリマップ取得 */
EFI_STATUS GetMemoryMap(struct MemoryMap* map) {
    /* メモリマップが用意されていない(小さすぎる)場合 */
    if(map->buffer == NULL){                    
        return EFI_BUFFER_TOO_SMALL;            // EFI_BUFFER_TOO_SMALLを返す
    }

    map->map_size = map->buffer_size;
    /*  UEFIのgBSテーブルに正式にマップ取得の依頼 */
    return gBS->GetMemoryMap(
        &map->map_size,                         // メモリ領域の大きさ
        (EFI_MEMORY_DESCRIPTOR*)map->buffer,    // メモリ領域の先頭ポインタ
        &map->map_key,                          // メモリマップの状態識別変数
        &map->descriptor_size,                  // メモリディスクリプタのバイト数
        &map->descriptor_version);              // メモリディスクリプタのバージョン番号
}

/* メモリタイプの数字から文字への翻訳 */
const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
    switch (type) {
        case EfiReservedMemoryType: return L"EfiReservedMemoryType";
        /* 今動かしているブートローダ自身の場所 */
        case EfiLoaderCode: return L"EfiLoaderCode";
        case EfiLoaderData: return L"EfiLoaderData";
        /* 今UEFIの機能が使っている場所．以降は使える */
        case EfiBootServicesCode: return L"EfiBootServicesCode";
        case EfiBootServicesData: return L"EfiBootServicesData";
        /* 今後もUEFIが使い続ける場所．上書き厳禁 */
        case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
        case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
        /* 空きメモリ */
        case EfiConventionalMemory: return L"EfiConventionalMemory";
        case EfiUnusableMemory: return L"EfiUnusableMemory";
        case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
        case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
        case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
        case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
        case EfiPalCode: return L"EfiPalCode";
        case EfiPersistentMemory: return L"EfiPersistentMemory";
        case EfiMaxMemoryType: return L"EfiMaxMemoryType";
        default: return L"InvalidMemoryType";
    }
}

/* メモリマップ情報をCSV形式で書き出す
   iterはiterator(配列・リスト等の複数要素を持つデータ構造)の略 */
EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file) {
    EFI_STATUS status;
    CHAR8 buf[256];
    UINTN len;

    /* 各要素の配置形式とそのサイズを取得してfileに書き込む */
    CHAR8* header = 
        "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
    len = AsciiStrLen(header);
    status = file->Write(file, &len, header);
    if(EFI_ERROR(status)) {
        return status;
    }

    Print(L"map->buffer = %08lx, map->map_size = %08lx\n",
        map->buffer, map->map_size);

    EFI_PHYSICAL_ADDRESS iter;
    int i;
    /* マップ全体をサイズごとに１要素としてそれぞれ書き出す */
    for(iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0;
        iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
        iter += map->descriptor_size, i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
        len = AsciiSPrint(
            buf, sizeof(buf),
            "%u, %x, %-ls, %08lx, %lx, %lx\n",
            i, desc->Type, GetMemoryTypeUnicode(desc->Type),
            desc->PhysicalStart, desc->NumberOfPages,
            desc->Attribute & 0xffffflu);
        status = file->Write(file, &len, buf); //すべて書き込めなかった場合のプログラムを書き込むべき
        if(EFI_ERROR(status)) {
            return status;
        }
    }

    return EFI_SUCCESS;
}

/* ルートディレクトリ取得 */
EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root) {
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

    status = gBS->OpenProtocol(
        image_handle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&loaded_image,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if(EFI_ERROR(status)) {
        return status;
    }
    status = gBS->OpenProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&fs,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if(EFI_ERROR(status)) {
        return status;
    }

    return fs->OpenVolume(fs, root);
}

/* 画像描画のインターフェース */
EFI_STATUS OpenGOP(EFI_HANDLE image_handle,
                   EFI_GRAPHICS_OUTPUT_PROTOCOL** gop) {
    EFI_STATUS status;
    UINTN num_gop_handles = 0;
    EFI_HANDLE* gop_handles = NULL;
    status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiGraphicsOutputProtocolGuid,    // 描画機能を持つものを探す
        NULL,
        &num_gop_handles,                   // 見つかった個数を入れる
        &gop_handles);                      // そのリストを入れる
    if(EFI_ERROR(status)) {
        return status;
    }
    status = gBS->OpenProtocol(
        gop_handles[0],                     // リストの最初のデバイスを使う
        &gEfiGraphicsOutputProtocolGuid,
        (VOID**)gop,                        // GOPの使い方を示す構造体
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if(EFI_ERROR(status)) {
        return status;
    }
    FreePool(gop_handles);                  //デバイスを取得したのでリストは不要なため解放

    return EFI_SUCCESS;
}

/* ピクセルフォーマットの数字から文字への変換 */
const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt) {
    switch (fmt) {
        case PixelRedGreenBlueReserved8BitPerColor:         // RGB型
            return L"PixelRedGreenBlueReserved8BitPerColor";
        case PixelBlueGreenRedReserved8BitPerColor:         // BGR型
            return L"PixelBlueGreenRedReserved8BitPerColor";
        case PixelBitMask:                                  // マスク値による計算が必要なタイプ
            return L"PixelBitMask";
        case PixelBltOnly:                                  // Blt関数経由でしか書けないタイプ
            return L"PixelBltOnly";
        case PixelFormatMax:                                // 境界値
            return L"PixelFormatMax";
        default:                                            // 不正な形式
            return L"InvalidPixelFormat";
    }
}

void Halt(void) {
    while (1) __asm__("hlt");
}

/* カーネルファイル内のすべてのLOADセグメントを探索し，最小値(first)と最大値(last)を求める */
void CalcLoadAddressRange(Elf64_Ehdr* ehdr, UINT64* first, UINT64* last) {
    Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
    *first = MAX_UINT64;
    *last = 0;
    for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i) {
        if(phdr[i].p_type != PT_LOAD) continue;
        *first = MIN(*first, phdr[i].p_vaddr);
        *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);
    }
}

/* 一時領域から最終目的地へコピー，余った領域を0でパディング */
void CopyLoadSegments(Elf64_Ehdr* ehdr){
    Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
    for(Elf64_Half i = 0; i < ehdr->e_phnum; ++i){
        if(phdr[i].p_type != PT_LOAD) continue;

        UINT64 segm_in_file = (UINT64)ehdr + phdr[i].p_offset;
        CopyMem((VOID*)phdr[i].p_vaddr, (VOID*)segm_in_file, phdr[i].p_filesz);

        UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
        SetMem((VOID*)(phdr[i].p_vaddr + phdr[i].p_filesz), remain_bytes, 0);
    }
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle, 
                           EFI_SYSTEM_TABLE *system_table) 
{
    EFI_STATUS status;
    Print(L"Hello, Mikan World!\n");            // 画面にメッセージを表示
    CHAR8 memmap_buf[4096 * 4];                 // メモリマップのためのバッファ用意
    struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0}; // MemoryMap構造体の作成と初期化
    status = GetMemoryMap(&memmap);                      // UEFIから現在のメモリマップを読み出す
    if(EFI_ERROR(status)) {
        Print(L"failed to get memory map: %r\n", status);
        Halt();
    }

    EFI_FILE_PROTOCOL* root_dir;
    status = OpenRootDir(image_handle, &root_dir);       // ディスクのrootを開く
    if(EFI_ERROR(status)) {
        Print(L"failed to open root directory: %r\n", status);
        Halt();
    }

    EFI_FILE_PROTOCOL* memmap_file;
    status = root_dir->Open(
        root_dir, &memmap_file, L"\\memmap",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);

    if (EFI_ERROR(status)) {
        Print(L"failed to open file '\\memmap': %r\n", status);
        Print(L"Ignored.\n");
    } else {
        status = SaveMemoryMap(&memmap, memmap_file);        // ファイルへの書き出し
        if(EFI_ERROR(status)){
            Print(L"failed to save memory map: %r\n", status);
            Halt();
        }
        status = memmap_file->Close(memmap_file);
        if(EFI_ERROR(status)){
            Print(L"failed to close memory map: %r\n", status);
            Halt();
        }
    }

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    status = OpenGOP(image_handle, &gop);                // UEFIからGOPを取得
    if(EFI_ERROR(status)){
        Print(L"failed to open GOP: %r\n", status);
        Halt();
    }
    /* 解像度，フォーマット，ピクセル数を表示 */
    Print(L"Resolution: %ux%u, Pixel Format: %s, %u pixels/line\n",
        gop->Mode->Info->HorizontalResolution,
        gop->Mode->Info->VerticalResolution,
        GetPixelFormatUnicode(gop->Mode->Info->PixelFormat),
        gop->Mode->Info->PixelsPerScanLine);
    /* フレームバッファのメモリ始端・終端，サイズの表示 */
    Print(L"Frame Buffer: 0x%0lx -0x%0lx, Size: %lu bytes\n",
        gop->Mode->FrameBufferBase,
        gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize,
        gop->Mode->FrameBufferSize);

    UINT8* frame_buffer = (UINT8*)gop->Mode->FrameBufferBase; //フレームバッファの始端を8ビット単位のポインタにキャスト
    /* フレームバッファの全容領分ループを回す */
    for (UINTN i = 0; i < gop->Mode->FrameBufferSize; ++i) {
        frame_buffer[i] = 255;
    }

    EFI_FILE_PROTOCOL* kernel_file;
    status = root_dir->Open(
        root_dir, &kernel_file, L"\\kernel.elf",
        EFI_FILE_MODE_READ, 0);
    if(EFI_ERROR(status)){
        Print(L"failed to open file '\\kernel.elf': %r\n", status);
        Halt();
    }
    
    /* EFI_FILE_INFOの最後のメンバは要素数が描かれていない配列である．
       そのため，最後のメンバFileNameに\kernel.elfという12バイトの文字列を格納するために
       (CHAR16) * 12して，構造体の大きさに足している */
    UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12; 
    UINT8 file_info_buffer[file_info_size];
    /* file_info_bufferにファイル情報を書き込む */
    status = kernel_file->GetInfo(
        kernel_file, &gEfiFileInfoGuid,
        &file_info_size, file_info_buffer);
    if(EFI_ERROR(status)){
        Print(L"failed to get file information: %r\n", status);
        Halt();
    }

    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer; // UINTNのただの数字からEFI_FILE_INFOとして意味を持つデータになるようキャストする．
    UINTN kernel_file_size = file_info->FileSize;

    VOID* kernel_buffer;
    status = gBS->AllocatePool(EfiLoaderData, kernel_file_size, &kernel_buffer); // カーネルファイルを読み込むための一時領域の確保
    if(EFI_ERROR(status)){
        Print(L"failed to allocate pool: %r\n", status);
        Halt();
    }

    status = kernel_file->Read(kernel_file, &kernel_file_size, kernel_buffer); // 実際に読み込み
    if(EFI_ERROR(status)){
        Print(L"error: %r", status);
        Halt();
    }
    
    Elf64_Ehdr* kernel_ehdr = (Elf64_Ehdr*)kernel_buffer;
    UINT64 kernel_first_addr, kernel_last_addr;
    CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr); // アドレスの始端・終端の取得

    UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;     // ページ数の計算
    /* メモリの確保，これまでは第３引数がkernel_file_addrであったが，
     * ファイルでの計算では.bssセクションが含まれず物理的にメモリが足りなるため，
     * これまでの計算でnum_pagesによって必要なページ数を適[切に計算した． 
     */
    status = gBS->AllocatePages(AllocateAddress, EfiLoaderData,
                                num_pages, &kernel_first_addr);              
    
    if(EFI_ERROR(status)){
        Print(L"failed to allocate pages: %r\n", status);
        Halt();
    }


    CopyLoadSegments(kernel_ehdr);  // 一時領域から最終目的地へLOADセグメントをコピーする
    Print(L"Kernel: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);

    status = gBS->FreePool(kernel_buffer);  // 一時領域の確保
    if(EFI_ERROR(status)){
        Print(L"failed to free pool: %r\n", status);
        Halt();
    }
    /* OSにとっては邪魔なため，今まで動いていたUEFI BIOSのブートサービスを停止 */
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    if (EFI_ERROR(status)){
        status = GetMemoryMap(&memmap);
        if(EFI_ERROR(status)){
            Print(L"failed to get memory map: %r\n", status);
            Halt();
        }
        status = gBS->ExitBootServices(image_handle, memmap.map_key);
        if(EFI_ERROR(status)){
            Print(L"Could not exit boot service: %r\n", status);
            Halt();
        }
    }

    /* エントリポイントの計算・呼び出し */
    UINT64 entry_addr = *(UINT64*)(kernel_first_addr + 24);//24バイトまでにはヘッダが記述されるため，エントリポイントとしては+24

    struct FrameBufferConfig config = {
        (UINT8*)gop->Mode->FrameBufferBase,
        gop->Mode->Info->PixelsPerScanLine,
        gop->Mode->Info->HorizontalResolution,
        gop->Mode->Info->VerticalResolution,
        0
    };

    switch (gop->Mode->Info->PixelFormat) {
        case PixelRedGreenBlueReserved8BitPerColor:
            config.pixel_format = kPixelRGBResv8BitPerColor;
            break;
        case PixelBlueGreenRedReserved8BitPerColor:
            config.pixel_format = kPixelBGRResv8BitPerColor;
            break;
        default:
            Print(L"Unimplemented pixel format: %d\n", gop->Mode->Info->PixelFormat);
            Halt();
    }

    VOID* acpi_table = NULL;
    for (UINTN i = 0; i < system_table->NumberOfTableEntries; ++i) {
        if (CompareGuid(&gEfiAcpiTableGuid,
                        &system_table->ConfigurationTable[i].VendorGuid)) {
            acpi_table = system_table->ConfigurationTable[i].VendorTable;
            break;
        }
    }

    typedef void EntryPointType(const struct FrameBufferConfig*,
                                const struct MemoryMap*,
                                const VOID*);
    EntryPointType* entry_point = (EntryPointType*)entry_addr; //そのような関数を指すポインタentry_pointにentry_addrを格納．
    entry_point(&config, &memmap, acpi_table); //entry_pointを呼び出し(entry_addrから実行)．

    Print(L"All done\n");

    while (1);                      // 無限ループで停止
    return EFI_SUCCESS;             // 正常終了を示すステータスコードを返す
}