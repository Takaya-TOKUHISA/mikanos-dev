#pragma once

#include "error.hpp"

/* 文字列を読み取れる何か */
class FileDescriptor {
    public:
        virtual ~FileDescriptor() = default;
        virtual size_t Read(void* buf, size_t len) = 0; // 純粋仮想関数
        virtual size_t Write(const void* buf, size_t len) = 0; // 純粋仮想関数
        virtual size_t Size() const = 0;

        /** @brief Loaf reads file content without changing internal offset */
        virtual size_t Load(void* buf, size_t len, size_t offset) = 0;
};