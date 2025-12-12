#include <mutex>
#include <vector>

#include "ultramodern/extensions.h"
#include "ultramodern/ultramodern.hpp"

struct DLEvent {
    PTR(OSMesgQueue) mq;
    OSMesg mesg;
    PTR(void) displaylist;
    u32 event_type;
};

struct {
    struct {
        std::mutex dl_event_mutex;
        std::vector<DLEvent> pending_events;
    } dl_events;
} extension_state;

extern "C" void osExQueueDisplaylistEvent(PTR(OSMesgQueue) mq, OSMesg mesg, PTR(void) displaylist, u32 event_type) {
    std::lock_guard lock{ extension_state.dl_events.dl_event_mutex };

    assert(
        event_type == OS_EX_DISPLAYLIST_EVENT_SUBMITTED || 
        event_type == OS_EX_DISPLAYLIST_EVENT_PARSED || 
        event_type == OS_EX_DISPLAYLIST_EVENT_COMPLETED);
    
    extension_state.dl_events.pending_events.emplace_back(
        DLEvent{ mq, mesg, displaylist, event_type }
    );
}

void dispatch_displaylist_events(PTR(void) displaylist, u32 event_type) {
    std::lock_guard lock{ extension_state.dl_events.dl_event_mutex };

    // Check every pending DL event to see if they match this displaylist and event type.
    for (auto iter = extension_state.dl_events.pending_events.begin(); iter != extension_state.dl_events.pending_events.end(); ) {
        if (iter->displaylist == displaylist && iter->event_type == event_type) {
            // Send the provided message to the corresponding message queue for this event, then remove this event from the queue.
            ultramodern::enqueue_external_message(iter->mq, iter->mesg, false, true);
            iter = extension_state.dl_events.pending_events.erase(iter);
        }
        else {
            ++iter;
        }
    }
}

void ultramodern::extensions::on_displaylist_submitted(PTR(void) displaylist) {
    dispatch_displaylist_events(displaylist, OS_EX_DISPLAYLIST_EVENT_SUBMITTED);
}

void ultramodern::extensions::on_displaylist_parsed(PTR(void) displaylist) {
    dispatch_displaylist_events(displaylist, OS_EX_DISPLAYLIST_EVENT_PARSED);
}

void ultramodern::extensions::on_displaylist_completed(PTR(void) displaylist) {
    dispatch_displaylist_events(displaylist, OS_EX_DISPLAYLIST_EVENT_COMPLETED);
}
