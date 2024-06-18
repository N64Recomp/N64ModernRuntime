#include <cstdio>

#include "ultramodern/error_handling.hpp"

static ultramodern::error_handling::callbacks_t error_handling_callbacks{};

void ultramodern::error_handling::set_callbacks(const ultramodern::error_handling::callbacks_t& callbacks) {
    error_handling_callbacks = callbacks;
}

void ultramodern::error_handling::message_box(const char* msg) {
    // We print the message to stderr since the user may not have provided a message_box callback

    fprintf(stderr, "%s\n", msg);

    if (error_handling_callbacks.message_box != nullptr) {
        error_handling_callbacks.message_box(msg);
    }
}

void ultramodern::error_handling::quick_exit(const char* filename, int line, const char *func, int exit_status) {
    fprintf(stderr, "Exiting with exit status '%i'. Function %s, at file %s:%i\n", exit_status, func, filename, line);

#ifdef __APPLE__
    std::_Exit(exit_status);
#else
    std::quick_exit(exit_status);
#endif
}
