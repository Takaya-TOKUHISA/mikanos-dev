/**
 * @file interrupt.cpp
 *
 * 割り込み用のプログラムを集めたファイル．
 */

#include "interrupt.hpp"

#include "asmfunc.h"
#include "segment.hpp"
#include "timer.hpp"
#include "task.hpp"

std::array<InterruptDescriptor, 256> idt;

/* 記述子に対して各種設定を行う */
void SetIDTEntry(InterruptDescriptor& desc,
                 InterruptDescriptorAttribute attr,
                 uint64_t offset,
                 uint16_t segment_selector) {
    desc.attr = attr;
    desc.offset_low = offset & 0xffffu;
    desc.offset_middle = (offset >> 16) & 0xffffu;
    desc.offset_high = offset >> 32;
    desc.segment_selector = segment_selector;
}

/* 0xfee000b0番地に値を書き込むことで割り込み処理の終了を知らせる */
void NotifyEndOfInterrupt() {
    volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
    *end_of_interrupt = 0;
}

namespace {

    __attribute__((interrupt))
    void IntHandlerXHCI(InterruptFrame* frame) {
        task_manager->SendMessage(1, Message{Message::kInterruptXHCI});
        NotifyEndOfInterrupt();
    }

    __attribute__((interrupt))
    void IntHandlerLAPICTimer(InterruptFrame* frame) {
        LAPICTimerOnInterrupt();
    }
}

void InitializeInterrupt(){
    SetIDTEntry(idt[InterruptVector::kXHCI],
                MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(IntHandlerXHCI),
                kKernelCS);
    SetIDTEntry(idt[InterruptVector::kLAPICTimer],
                MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(IntHandlerLAPICTimer),
                kKernelCS);
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}