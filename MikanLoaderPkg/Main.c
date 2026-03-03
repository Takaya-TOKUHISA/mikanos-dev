#include <Uefi.h>                               // UEFIの基本的な型や定数を定義(EFI_STATUSなど)
#include <Library/UefiLib.h>                    // UEFIのライブラリ関数を定義(Printなど)
#include <Library/UefiBootServicesTableLib.h>   // UEFIのブートサービステーブルを定義(gBSなど)
#include <Library/PrintLib.h>                   // 高度なPrint関数を定義
#include <Protocol/LoadedImage.h>               // ロードされたイメージに関するプロトコルを定義
#include <Protocol/SimpleFileSystem.h>          // ファイル操作の機能提供
#include <Protocol/DiskIo2.h>                   // 非同期なオフセット指定での読み書きを提供
#include <Protocol/BlockIo.h>                   // ブロック単位での読み書きを提供(512B等)
#include <Guid/FileInfo.h>

struct MemoryMap {
    UINTN buffer_size;                          // メモリマップ全体のサイズ
    VOID* buffer;                               // 実際のメモリマップ・データの先頭アドレス  
    UINTN map_size;                             // 書き込んだサイズ
    UINTN map_key;                              // メモリの状態を表す番号
    UINTN descriptor_size;                      // 一つのメモリ領域ごとのサイズ
    UINT32 descriptor_version;                  // 記録フォーマットのバージョン
};


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
    CHAR8 buf[256];
    UINTN len;

    /* 各要素の配置形式とそのサイズを取得してfileに書き込む */
    CHAR8* header = 
        "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
    len = AsciiStrLen(header);
    file->Write(file, &len, header);

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
        file->Write(file, &len, buf); //すべて書き込めなかった場合のプログラムを書き込むべき
    }

    return EFI_SUCCESS;
}

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root) {
    EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

    gBS->OpenProtocol(
        image_handle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&loaded_image,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    
    gBS->OpenProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&fs,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    fs->OpenVolume(fs, root);

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle, 
                           EFI_SYSTEM_TABLE *system_table) 
{
    Print(L"Hello, Mikan World!\n");// 画面にメッセージを表示
    CHAR8 memmap_buf[4096 * 4];
    struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
    GetMemoryMap(&memmap);

    EFI_FILE_PROTOCOL* root_dir;
    OpenRootDir(image_handle, &root_dir);

    EFI_FILE_PROTOCOL* memmap_file;
    root_dir->Open(
        root_dir, &memmap_file, L"\\memmap",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);

    SaveMemoryMap(&memmap, memmap_file);
    memmap_file->Close(memmap_file);


    EFI_FILE_PROTOCOL* kernel_file;
    root_dir->Open(
        root_dir, &kernel_file, L"\\kernel.elf",
        EFI_FILE_MODE_READ, 0);
    /* EFI_FILE_INFOの最後のメンバは要素数が描かれていない配列である．
       そのため，最後のメンバFileNameに\kernel.elfという12バイトの文字列を格納するために
       (CHAR16) * 12して，構造体の大きさに足している */
    UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12; 
    UINTN file_info_buffer[file_info_size];
    /* file_info_bufferにファイル情報を書き込む */
    kernel_file->GetInfo(
        kernel_file, &gEfiFileInfoGuid,
        &file_info_size, file_info_buffer);
    
    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer; //UINTNのただの数字からEFI_FILE_INFOとして意味を持つデータになるようキャストする．
    UINTN kernel_file_size = file_info->FileSize;

    EFI_PHYSICAL_ADDRESS kernel_base_addr = 0x100000;
    /* 
    ファイルを十分に格納できるメモリ領域を確保する．
    カーネルファイルを0x10000番地前提で作っているので，0x10000に指定して配置する．
    ページ数 = (kernel_file_size + 0xfff) / 0x1000 
    0x1000B = 4KiB:1ページの大きさ
    0xfffはページサイズに合わせて端数の切り捨てを防ぐため
    0x1000 - 1 = 0xfff
    */
    gBS->AllocatePages(
        AllocateAddress, EfiLoaderData,
        (kernel_file_size + 0xfff) / 0x1000, &kernel_base_addr);
    kernel_file->Read(kernel_file, &kernel_file_size, (VOID*)kernel_base_addr);
    Print(L"Kernel: 0x%01x (%lu bytes)\n", kernel_base_addr, kernel_file_size);

    /* OSにとっては邪魔なため，今まで動いていたUEFI BIOSのブートサービスを停止 */
    EFI_STATUS status;
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    if (EFI_ERROR(status)){
        status = GetMemoryMap(&memmap);
        if(EFI_ERROR(status)){
            Print(L"failed to get memory map: %r\n", status);
            while(1);
        }
        status = gBS->ExitBootServices(image_handle, memmap.map_key);
        if(EFI_ERROR(status)){
            Print(L"Could not exit boot service: %r\n", status);
            while(1);
        }
    }

    /* エントリポイントの計算・呼び出し */
    UINT64 entry_addr = *(UINT64*)(kernel_base_addr + 24);//24バイトまでにはヘッダが記述されるため，エントリポイントとしては+24

    typedef void EntryPointType(void);//引数・返り値ともにvoidな変数の型を定義
    EntryPointType* entry_point = (EntryPointType*)entry_addr; //そのような関数を指すポインタentry_pointにentry_addrを格納
    entry_point(); //entry_pointを呼び出し(entry_addrから実行)

    Print(L"ALL done\n");

    while (1);                      // 無限ループで停止
    return EFI_SUCCESS;             // 正常終了を示すステータスコードを返す
}