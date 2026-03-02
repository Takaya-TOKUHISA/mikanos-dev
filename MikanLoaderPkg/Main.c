#include <Uefi.h>                   // UEFIの基本的な型や定数を定義(EFI_STATUSなど)
#include <Library/UefiLib.h>        // UEFIのライブラリ関数を定義(Printなど)

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle, 
                           EFI_SYSTEM_TABLE *system_table) 
{
    Print(L"Hello, Mikan World!\n");// 画面にメッセージを表示
    while (1);                       // 無限ループで停止
    return EFI_SUCCESS;             // 正常終了を示すステータスコードを返す
}