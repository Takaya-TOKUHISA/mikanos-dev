#pragma once

#include <stdint.h>

typedef uintptr_t Elf64_Addr;   // メモリアドレス
typedef uint64_t  Elf64_Off;    // ファイル内のオフセット位置
typedef uint16_t  Elf64_Half;   // 半分のWord
typedef uint32_t  Elf64_Word;   // 通常Word
typedef int32_t   Elf64_Sword;  // 符号ありWord
typedef uint64_t  Elf64_Xword;  // 拡張Word
typedef int64_t   Elf64_Sxword; // 符号あり拡張Word

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half  e_type;     // ファイルの種類
    Elf64_Half  e_machine;  // CPUのアーキテクチャ
    Elf64_Word  e_version;  // ELFのバージョン
    Elf64_Addr  e_entry;    // 最初に実行すべきメモリアドレス
    Elf64_Off   e_phoff;    // プログラムヘッダの開始位置
    Elf64_Off   e_shoff;    // セクションヘッダの開始位置
    Elf64_Word  e_flags;    // プロセッサ特有のフラグ
    Elf64_Half  e_ehsize;   // このELFヘッダ自体のサイズ
    Elf64_Half  e_phentsize;// プログラムヘッダ1つ分のサイズ
    Elf64_Half  e_phnum;    // プログラムヘッダの個数
    Elf64_Half  e_shentsize;// セクションヘッダ1つ分のサイズ
    Elf64_Half  e_shnum;    // セクションヘッダの個数 
    Elf64_Half  e_shstrndx; // セクション名が格納されているインデックス
} Elf64_Ehdr;

#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3
#define ET_CORE 4

typedef struct {
    Elf64_Word  p_type;     // PHDR，LOAD などのセグメント種別
    Elf64_Word  p_flags;    // フラグ
    Elf64_Off   p_offset;   // オフセット
    Elf64_Addr  p_vaddr;    // 仮想 Addr
    Elf64_Addr  p_paddr;    // 物理 Addr
    Elf64_Xword p_filesz;   // ファイルサイズ
    Elf64_Xword p_memsz;    // メモリサイズ
    Elf64_Xword p_align;    // アラインメント(境界合わせ)
} Elf64_Phdr;

#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_NOTE     4
#define PT_SHLIB    5
#define PT_PHDR     6
#define PT_TLS      7

typedef struct {
    Elf64_Sxword d_tag;     // 動的情報の種類
    union {
        Elf64_Xword d_val;  // 数値データ
        Elf64_Addr  d_ptr;  // アドレスデータ
    } d_un;
} Elf64_Dyn;

#define DT_NULL     0
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9

typedef struct {
    Elf64_Addr   r_offset;  // 再配置が必要なメモリの場所
    Elf64_Xword  r_info;    // 再配置の種類とシンボル情報
    Elf64_Sxword r_addend;  // 書き換える際に足し算する定数
} Elf64_Rela;

//r_infoから情報を取り出すためのマクロ
#define ELF64_R_SYM(i)      ((i)>>32)
#define ELF64_R_TYPE(i)     ((i)&0xffffffffL)
#define ELF64_R_INFO(s,t)   (((s)<<32)+((t)&0xffffffffL))

#define R_X86_64_RELATIVE 8