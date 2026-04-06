#include "clipboard.hpp"

#include <cstring>

ClipArea::ClipArea() : selecting_{false}, start{-1}, end{-1} {}

void ClipArea::SelectArea(int pos, int direction) {
    if (selecting_ == false) {
        selecting_ = true;
        start = std::min(pos, pos+direction);
        end = std::min(pos, pos+direction);
    } else {
        end += direction;
        if (start == end) {
            FreeArea();
        }
    }
}

void ClipArea::FreeArea() {
    if (selecting_) {
        selecting_ = false;
        start = -1;
        end = -1;
    }
}

ClipBoard* clip_board = nullptr;

ClipBoard::ClipBoard(unsigned int capacity) : 
    capacity_{capacity}, len_{0}, clip_board_{new char[capacity]} {
}

ClipBoard::~ClipBoard() {
    delete[] clip_board_;
}

unsigned int ClipBoard::Length() const {
    return len_;
}

unsigned int ClipBoard::Capacity() const {
    return capacity_;
}

Error ClipBoard::Clear() {
    memset(clip_board_, 0, capacity_);
    len_ = 0;
    return MAKE_ERROR(Error::kSuccess);
}

Error ClipBoard::CopyString(const char* src, unsigned int len) {
    if (src == nullptr) {
        return MAKE_ERROR(Error::kInvalidArgument);
    }
    if (len > capacity_) {
        return MAKE_ERROR(Error::kBufferTooSmall);
    }

    memcpy(clip_board_, src, len);
    len_ = len;
    return MAKE_ERROR(Error::kSuccess);
}

WithError<char*> ClipBoard::PasteString(char* dst, unsigned int len) {
    if (dst == nullptr || len == 0) {
        return { nullptr, MAKE_ERROR(Error::kInvalidArgument) };
    }
    if (len <= len_) {
        return { nullptr, MAKE_ERROR(Error::kBufferTooSmall) };
    }

    memcpy(dst, clip_board_, len_);
    dst[len_] = 0;
    return { dst, MAKE_ERROR(Error::kSuccess) };
}

void InitializeClipBoard() {
    clip_board = new ClipBoard();
}