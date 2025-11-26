#include "gait.h"

void gait_init() {
    // TODO: initialize step detection / cadence tracking here
}

GaitStatus gait_update(const SignalWindow &window) {
    (void)window; // avoid unused warning for now

    GaitStatus status{};
    status.fog_state = 0;  // no FOG detected yet
    return status;
}
