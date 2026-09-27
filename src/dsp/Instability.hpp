#pragma once
#include <cmath>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace garmire {

// ─────────────────────────────────────────────────────────────────────
//  DriftGenerator — Filtered random walk for analog-style instability
//
//  Produces a smooth, slowly-varying random signal in [-1, 1].
//  Uses an LCG for noise and a 1-pole lowpass for smoothness.
//  Each instance has its own seed for independent per-channel drift.
// ─────────────────────────────────────────────────────────────────────
struct DriftGenerator {
	float state = 0.f;         // Lowpass filter state (output)
	uint32_t seed = 12345u;    // LCG seed — set per instance

	// Initialize with a unique seed
	void init(uint32_t s) {
		seed = s ? s : 1u;     // Avoid zero seed
		state = 0.f;
	}

	// Process one sample.
	//   rate: 0–1 controls drift speed (0 = frozen, 1 = fast jitter)
	//   Returns: smooth random value in approximately [-1, 1]
	float process(float deltaTime, float rate) {
		// LCG noise source (Numerical Recipes constants)
		seed = seed * 1664525u + 1013904223u;
		float noise = (float)((int32_t)seed) / 2147483648.f;  // [-1, 1)

		// 1-pole lowpass: cutoff scales quadratically with rate for
		// more usable range (mostly slow drift, fast only at extreme)
		float cutoff = 0.005f + rate * rate * 4.f;   // ~0.005 Hz to ~4 Hz
		float alpha = 2.f * (float)M_PI * cutoff * deltaTime;
		if (alpha > 1.f) alpha = 1.f;

		state += alpha * (noise - state);
		return state;
	}
};

} // namespace garmire
