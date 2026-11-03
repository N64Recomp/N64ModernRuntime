#include <thread>
#include <variant>
#include <map>
#include <unordered_map>
#include <utility>
#include "blockingconcurrentqueue.h"

#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#endif

// Start time for the program
static std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();
// Offset of the duration since program start used to calculate the value for osGetTime. 
static int64_t ostime_offset = 0;
// Game speed multiplier (1 means no speedup)
constexpr uint32_t speed_multiplier = 1;
// N64 CPU counter ticks per millisecond
constexpr uint32_t counter_per_ms = 46'875 * speed_multiplier;

struct OSTimer {
    PTR(OSTimer) unused1;
    PTR(OSTimer) unused2;
    OSTime interval;
    OSTime timestamp;
    PTR(OSMesgQueue) mq;
    OSMesg msg;
};

struct TimerState {
    OSTime timestamp;
    OSTime interval;
    PTR(OSMesgQueue) mq;
    OSMesg msg;
};

struct AddTimerAction {
    PTR(OSTimer) timer;
    TimerState state;
};

struct RemoveTimerAction {
    PTR(OSTimer) timer;
};

using Action = std::variant<AddTimerAction, RemoveTimerAction>;

struct {
    std::thread thread;
    moodycamel::BlockingConcurrentQueue<Action> action_queue{};
} timer_context;

uint64_t duration_to_ticks(std::chrono::high_resolution_clock::duration duration) {
    uint64_t delta_micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    // More accurate than using a floating point timer, will only overflow after running for 12.47 years
    // Units: (micros * (counts/millis)) / (micros/millis) = counts
    uint64_t total_count = (delta_micros * counter_per_ms) / 1000;

    return total_count;
}

std::chrono::microseconds ticks_to_duration(uint64_t ticks) {
    using namespace std::chrono_literals;
    return ticks * 1000us / counter_per_ms;
}

std::chrono::high_resolution_clock::time_point ticks_to_timepoint(uint64_t ticks) {
    return start_time + ticks_to_duration(ticks);
}

uint64_t time_now() {
    return duration_to_ticks(std::chrono::high_resolution_clock::now() - start_time);
}

using TimerKey = std::pair<OSTime, PTR(OSTimer)>;

void timer_thread(RDRAM_ARG1) {
    ultramodern::set_native_thread_name("Timer Thread");
    ultramodern::set_native_thread_priority(ultramodern::ThreadPriority::VeryHigh);
    std::map<TimerKey, TimerState> active_timers{};
    std::unordered_map<PTR(OSTimer), OSTime> active_timer_timestamps{};

    //add and remove a timer
    auto remove_timer = [&](PTR(OSTimer) timer) {
        auto timestamp_it = active_timer_timestamps.find(timer);
        if (timestamp_it != active_timer_timestamps.end()) {
            active_timers.erase(TimerKey{timestamp_it->second, timer});
            active_timer_timestamps.erase(timestamp_it);
        }
    };

    auto insert_timer = [&](PTR(OSTimer) timer, const TimerState& state) {
        remove_timer(timer);
        active_timers.emplace(TimerKey{state.timestamp, timer}, state);
        active_timer_timestamps.emplace(timer, state.timestamp);
    };

    // Lambda to process a timer action to handle adding and removing timers
    auto process_timer_action = [&](const Action& action) {
        // Determine the action type and act on it
        if (const auto* add_action = std::get_if<AddTimerAction>(&action)) {
            insert_timer(add_action->timer, add_action->state);
        } else if (const auto* remove_action = std::get_if<RemoveTimerAction>(&action)) {
            remove_timer(remove_action->timer);
        }
    };

    while (true) {
        // Empty the action queue
        Action cur_action;
        while (timer_context.action_queue.try_dequeue(cur_action)) {
            process_timer_action(cur_action);
        }

        // If there's no timer to act on, wait for one to come in from the action queue
        while (active_timers.empty()) {
            timer_context.action_queue.wait_dequeue(cur_action);
            process_timer_action(cur_action);
        }

        // Get the timer that's closest to running out
        const auto cur_timer_it = active_timers.begin();
        PTR(OSTimer) cur_timer_ = cur_timer_it->first.second;
        TimerState cur_timer = cur_timer_it->second;

        // Remove the timer from the queue (it may get readded if waiting is interrupted)
        active_timers.erase(cur_timer_it);
        active_timer_timestamps.erase(cur_timer_);

        // Determine how long to wait to reach the timer's timestamp
        auto wait_duration = ticks_to_timepoint(cur_timer.timestamp) - std::chrono::high_resolution_clock::now();

        // Wait for either the duration to complete or a new action to come through
        if (wait_duration.count() >= 0 && timer_context.action_queue.wait_dequeue_timed(cur_action, wait_duration)) {
            // Timer was interrupted by a new action 
            // Add the current timer back to the queue (done first in case the action is to remove this timer)
            insert_timer(cur_timer_, cur_timer);
            // Process the new action
            process_timer_action(cur_action);
        }
        else {
            // Waiting for the timer completed, so send the timer's message to its message queue
            ultramodern::enqueue_external_message_src(cur_timer.mq, cur_timer.msg, false, ultramodern::EventMessageSource::Timer);
            // If the timer has a specified interval then reload it with that value
            if (cur_timer.interval != 0) {
                cur_timer.timestamp = cur_timer.interval + time_now();
                insert_timer(cur_timer_, cur_timer);
            }
        }
    }
}

void ultramodern::init_timers(RDRAM_ARG1) {
    timer_context.thread = std::thread{ timer_thread, PASS_RDRAM1 };
    timer_context.thread.detach();
}

uint32_t ultramodern::get_speed_multiplier() {
    return speed_multiplier;
}

std::chrono::high_resolution_clock::time_point ultramodern::get_start() {
    return start_time;
}

std::chrono::high_resolution_clock::duration ultramodern::time_since_start() {
    return std::chrono::high_resolution_clock::now() - start_time;
}

extern "C" u32 osGetCount() {
    uint64_t total_count = time_now();

    // Allow for overflows, which is how osGetCount behaves
    return (uint32_t)total_count;
}

extern "C" void osSetCount(u32 count) {
    assert(false);
}

extern "C" OSTime osGetTime() {
    uint64_t total_count = time_now() - ostime_offset;

    return total_count;
}

extern "C" void osSetTime(OSTime t) {
    ostime_offset = time_now() - t;
}

extern "C" int osSetTimer(RDRAM_ARG PTR(OSTimer) t_, OSTime countdown, OSTime interval, PTR(OSMesgQueue) mq, OSMesg msg) {
    OSTimer* t = TO_PTR(OSTimer, t_);

    // Determine the time when this timer will trigger off
    OSTime timestamp;
    if (countdown == 0) {
        // Set the timestamp based on the interval
        timestamp = interval + time_now();
    } else {
        timestamp = countdown + time_now();
    }

    t->timestamp = timestamp;
    t->interval = interval;
    t->mq = mq;
    t->msg = msg;

    timer_context.action_queue.enqueue(AddTimerAction{ t_, TimerState{ timestamp, interval, mq, msg } });

    return 0;
}

extern "C" int osStopTimer(RDRAM_ARG PTR(OSTimer) t_) {
    timer_context.action_queue.enqueue(RemoveTimerAction{ t_ });

    // TODO don't blindly return 0 here; requires some response from the timer thread to know what the returned value was
    return 0;
}

#ifdef _WIN32

// The implementations of std::chrono::sleep_until and sleep_for were affected by changing the system clock backwards in older versions
// of Microsoft's STL. This was fixed as of Visual Studio 2022 17.9, but to be safe ultramodern uses Win32 Sleep directly.
void ultramodern::sleep_milliseconds(uint32_t millis) {
    Sleep(millis);
}

void ultramodern::sleep_until(const std::chrono::high_resolution_clock::time_point& time_point) {
    auto time_now = std::chrono::high_resolution_clock::now();
    if (time_point > time_now) {
        long long delta_ms = std::chrono::ceil<std::chrono::milliseconds>(time_point - time_now).count();
        // printf("Sleeping %lld %d ms\n", delta_ms, (uint32_t)delta_ms);
        Sleep(delta_ms);
    }
}

#else

void ultramodern::sleep_milliseconds(uint32_t millis) {
    std::this_thread::sleep_for(std::chrono::milliseconds{millis});
}

void ultramodern::sleep_until(const std::chrono::high_resolution_clock::time_point& time_point) {
    std::this_thread::sleep_until(time_point);
}

#endif
