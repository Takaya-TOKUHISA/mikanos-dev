#pragma once

#include "error.hpp"

class ClipBoard {
    public:
        ClipBoard(unsigned int capacity = 4095);
        ~ClipBoard();

        unsigned int Length() const;
        unsigned int Capacity() const;


        Error Clear();
        Error CopyString(const char* src, unsigned int len);
        WithError<char*> PasteString(char* dst, unsigned int len);

    private:
        unsigned int capacity_;  // 最大サイズ
        unsigned int len_;  // 現状の有効サイズ
        char* clip_board_;  // ボード本体 
};

extern ClipBoard* clip_board;

void InitializeClipBoard();