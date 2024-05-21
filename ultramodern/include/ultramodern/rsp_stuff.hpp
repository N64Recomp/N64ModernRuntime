#ifndef __RSP_STUFF_HPP__
#define __RSP_STUFF_HPP__

// TODO: rename

#include <cstdint>

// TODO: Move these to ultramodern namespace?

enum class RspExitReason {
    Invalid,
    Broke,
    ImemOverrun,
    UnhandledJumpTarget,
    Unsupported
};

using RspUcodeFunc = RspExitReason(uint8_t* rdram);

extern uint8_t dmem[];
extern uint16_t rspReciprocals[512];
extern uint16_t rspInverseSquareRoots[512];

#endif
