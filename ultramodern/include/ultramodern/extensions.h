#ifndef __ULTRAMODERN_EXTENSIONS_H__
#define __ULTRAMODERN_EXTENSIONS_H__

#if defined(mips) // Patch compilation
#include <ultra64.h>
#else
#include "ultramodern/ultra64.h"
#endif

#ifndef PTR
#define PTR(x) x*
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    // Triggers when a displaylist has been submitted to the renderer.
    OS_EX_DISPLAYLIST_EVENT_SUBMITTED,
    // Triggers when a displaylist has been fully parsed by the renderer. This includes any referenced data, such
    // as vertices, matrices, and textures.
    OS_EX_DISPLAYLIST_EVENT_PARSED,
    // Triggers when rendering of a displaylist has been completed by the renderer. This only includes the
    // rendering pass that produces an image in RAM, not the high res output images that get presented to the user.
    OS_EX_DISPLAYLIST_EVENT_COMPLETED
} DisplaylistEventType;

// Queues a one-time message to be sent the next time the given event type occurs for the given displaylist.
// This allows easier detection of displaylist events without needing to patch a game's scheduler.
// The event will be cleared after it occurs.
// event_type must be a member of the DisplaylistEventType enum.
void osExQueueDisplaylistEvent(PTR(OSMesgQueue) mq, OSMesg mesg, PTR(void) displaylist, u32 event_type);

#ifdef __cplusplus
}

namespace ultramodern {
    namespace extensions {
        void on_displaylist_submitted(PTR(u64) displaylist);
        void on_displaylist_parsed(PTR(u64) displaylist);
        void on_displaylist_completed(PTR(u64) displaylist);
    }
}

#endif

#endif
