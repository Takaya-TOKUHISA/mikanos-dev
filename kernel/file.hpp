#pragma once

/* 文字列を読み取れる何か */
class FileDescriptor {
    public:
        virtual ~FileDescriptor() = default;
        virtual size_t Read(void* buf, size_t len) = 0; // 純粋仮想関数
};