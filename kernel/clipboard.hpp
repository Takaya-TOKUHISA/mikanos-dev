#pragma once

#include "error.hpp"

class ClipArea {
    public:
        ClipArea();
        
        void SelectArea(int pos, int direction);
        void FreeArea();
        int Start() const { return start; }
        int End() const { return end; }

    private:
        bool selecting_;
        int start;
        int end;
};

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