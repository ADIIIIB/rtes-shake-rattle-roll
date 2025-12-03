#include "gait.h"
#include <cmath>

void gait_init() {
    // Nothing to initialize for now
}

static float compute_rms(const SignalWindow &window) {
    if (window.length == 0) return 0.0f;

    float sum2 = 0.0f;
    for (size_t i = 0; i < window.length; ++i) {
        float x = window.data[i];
        sum2 += x * x;
    }
    return std::sqrt(sum2 / window.length);
}

GaitStatus gait_update(const SignalWindow &window) {
    GaitStatus g{};
    const size_t N = window.length;
    if (N == 0) {
        g.fog_state      = 0;
        g.step_rate_spm  = 0;
        g.variability    = 0;
        return g;
    }

    // 1) RMS as a rough "variability / intensity" measure
    float rms = compute_rms(window);

    float var_score = rms * 50.0f;  // tunable scaling
    if (var_score < 0.0f)   var_score = 0.0f;
    if (var_score > 100.0f) var_score = 100.0f;
    g.variability = static_cast<uint8_t>(var_score + 0.5f);

    // 2) Simple step detector: threshold crossings
    const float STEP_THR = 0.15f;   // tune if needed
    int steps = 0;
    bool above_prev = false;

    for (size_t n = 0; n < N; ++n) {
        float x = window.data[n];
        bool above = (x >= STEP_THR);
        if (above && !above_prev) {
            steps++;
        }
        above_prev = above;
    }

    float step_rate_f = 0.0f;
    if (WINDOW_SEC > 0.0f) {
        step_rate_f = static_cast<float>(steps) / WINDOW_SEC * 60.0f; // steps per minute
    }

    // 3) Interpret step rate for demo:
    //
    // - Very low RMS or very low step rate → not walking.
    // - "Normal walking" band: ~30–120 spm → show actual step_rate.
    // - Extremely high rates (>120–140 spm) → likely tremor / artifact,
    //   not gait → for demo, show 0 steps (standing with tremor).
    const float RMS_WALK_MIN      = 0.05f;  // below this, assume standing
    const float WALK_MIN_SPM      = 30.0f;
    const float WALK_MAX_SPM      = 120.0f;
    const float ARTIFACT_SPM_THR  = 140.0f;

    uint8_t step_rate_out = 0;

    if (rms < RMS_WALK_MIN) {
        // Very little motion → not walking
        step_rate_out = 0;
    } else if (step_rate_f >= WALK_MIN_SPM && step_rate_f <= WALK_MAX_SPM) {
        // Reasonable walking range → keep it
        if (step_rate_f < 0.0f)   step_rate_f = 0.0f;
        if (step_rate_f > 255.0f) step_rate_f = 255.0f;
        step_rate_out = static_cast<uint8_t>(step_rate_f + 0.5f);
    } else if (step_rate_f > ARTIFACT_SPM_THR) {
        // Unrealistically high → treat as tremor, not gait
        step_rate_out = 0;
    } else {
        // Between 10 and 30 spm, or 120–140 spm → ambiguous,
        // keep but clamp gently into 30–120 for demo.
        float clamped = step_rate_f;
        if (clamped < WALK_MIN_SPM) clamped = WALK_MIN_SPM;
        if (clamped > WALK_MAX_SPM) clamped = WALK_MAX_SPM;
        step_rate_out = static_cast<uint8_t>(clamped + 0.5f);
    }

    g.step_rate_spm = step_rate_out;

    // 4) FOG heuristic:
    //
    // Only consider possible FOG when we are in the "walking" band
    // AND RMS is very low → stepping intention but little movement.
    uint8_t fog = 0;
    if (g.step_rate_spm >= WALK_MIN_SPM && g.step_rate_spm <= WALK_MAX_SPM) {
        if (rms < 0.06f) {
            fog = 1;
        }
    }

    g.fog_state = fog;
    return g;
}