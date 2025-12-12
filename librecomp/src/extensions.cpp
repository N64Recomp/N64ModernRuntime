#include "recomp.h"
#include "ultramodern/extensions.h"
#include "librecomp/helpers.hpp"

extern "C" void osExQueueDisplaylistEvent_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSMesgQueue) mq   = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    OSMesg mesg           = _arg<1, OSMesg>(rdram, ctx);
    PTR(void) displaylist = _arg<2, PTR(void)>(rdram, ctx);
    u32 event_type        = _arg<3, u32>(rdram, ctx);
    osExQueueDisplaylistEvent(mq, mesg, displaylist, event_type);
}
