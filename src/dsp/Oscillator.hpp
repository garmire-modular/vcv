#pragma once
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace garmire {

// ─── Clamp helper (C++11 compatible) ────────────────────────────────
inline float clampf(float x, float lo, float hi) {
	return (x < lo) ? lo : (x > hi) ? hi : x;
}

// ─────────────────────────────────────────────────────────────────────
//  Waveform morph (bipolar output [-1, 1])
//
//  morph 0.00 = Sine
//  morph 0.25 = Triangle
//  morph 0.50 = Saw
//  morph 0.75 = Square
//  morph 1.00 = Pulse (10% duty)
//
//  Between positions: linear crossfade
// ─────────────────────────────────────────────────────────────────────
inline float waveformMorph(float phase, float morph) {
	morph = clampf(morph, 0.f, 1.f);

	// Base waveforms from phase ∈ [0, 1)
	float sine     = std::sin(2.f * (float)M_PI * phase);
	float triangle = 4.f * std::fabs(phase - 0.5f) - 1.f;
	float saw      = 2.f * phase - 1.f;
	float square   = (phase < 0.5f) ? 1.f : -1.f;
	float pulse    = (phase < 0.1f) ? 1.f : -1.f;

	// 4-segment crossfade
	if (morph < 0.25f) {
		float t = morph * 4.f;
		return sine * (1.f - t) + triangle * t;
	} else if (morph < 0.5f) {
		float t = (morph - 0.25f) * 4.f;
		return triangle * (1.f - t) + saw * t;
	} else if (morph < 0.75f) {
		float t = (morph - 0.5f) * 4.f;
		return saw * (1.f - t) + square * t;
	} else {
		float t = (morph - 0.75f) * 4.f;
		return square * (1.f - t) + pulse * t;
	}
}

// ─────────────────────────────────────────────────────────────────────
//  Waveform name string for tooltip
//  Returns e.g. "Saw 65% — Triangle 35%"
// ─────────────────────────────────────────────────────────────────────
inline std::string waveformName(float morph) {
	morph = clampf(morph, 0.f, 1.f);

	const char* names[] = {"Sine", "Triangle", "Saw", "Square", "Pulse"};
	float positions[] = {0.f, 0.25f, 0.5f, 0.75f, 1.f};

	if (morph >= 1.f) return "Pulse 100%";

	int idx = 0;
	for (int i = 0; i < 4; i++) {
		if (morph >= positions[i] && morph < positions[i + 1]) {
			idx = i;
			break;
		}
	}

	float segLen = positions[idx + 1] - positions[idx];
	float t = (morph - positions[idx]) / segLen;
	int pctB = (int)(t * 100.f + 0.5f);
	int pctA = 100 - pctB;

	if (pctA >= 100) return std::string(names[idx]) + " 100%";
	if (pctB >= 100) return std::string(names[idx + 1]) + " 100%";
	return std::string(names[idx]) + " " + std::to_string(pctA) + "% \xe2\x80\x94 " +
	       std::string(names[idx + 1]) + " " + std::to_string(pctB) + "%";
}

// ─────────────────────────────────────────────────────────────────────
//  Blanking envelope (unipolar output [0, 1])
//
//  shape 0.0 = Saw down  |\ (fast attack, slow decay)
//  shape 0.5 = Square   |‾|_ (on/off)
//  shape 1.0 = Saw up    /| (slow attack, fast decay)
//
//  Between positions: linear crossfade
// ─────────────────────────────────────────────────────────────────────
inline float blankingEnvelope(float phase, float shape) {
	shape = clampf(shape, 0.f, 1.f);

	float sawDown = 1.f - phase;                        // |\  decay
	float sq      = (phase < 0.5f) ? 1.f : 0.f;        // |‾|_
	float sawUp   = phase;                              // /|  attack

	if (shape < 0.5f) {
		float t = shape * 2.f;
		return sawDown * (1.f - t) + sq * t;
	} else {
		float t = (shape - 0.5f) * 2.f;
		return sq * (1.f - t) + sawUp * t;
	}
}

// ─────────────────────────────────────────────────────────────────────
//  Blanking shape name string for tooltip
// ─────────────────────────────────────────────────────────────────────
inline std::string blankingShapeName(float shape) {
	shape = clampf(shape, 0.f, 1.f);

	const char* names[] = {"Saw Down", "Square", "Saw Up"};
	float positions[] = {0.f, 0.5f, 1.f};

	if (shape >= 1.f) return "Saw Up 100%";

	int idx = (shape < 0.5f) ? 0 : 1;

	float segLen = positions[idx + 1] - positions[idx];
	float t = (shape - positions[idx]) / segLen;
	int pctB = (int)(t * 100.f + 0.5f);
	int pctA = 100 - pctB;

	if (pctA >= 100) return std::string(names[idx]) + " 100%";
	if (pctB >= 100) return std::string(names[idx + 1]) + " 100%";
	return std::string(names[idx]) + " " + std::to_string(pctA) + "% \xe2\x80\x94 " +
	       std::string(names[idx + 1]) + " " + std::to_string(pctB) + "%";
}

} // namespace garmire
