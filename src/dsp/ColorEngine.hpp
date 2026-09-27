#pragma once
#include <cmath>

namespace garmire {
namespace color {

// ─── HSV → RGB ──────────────────────────────────────────────────────
// H ∈ [0, 360), S ∈ [0, 1], V ∈ [0, 1]
// Output R, G, B ∈ [0, 1]
inline void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
	if (s <= 0.0001f) { r = g = b = v; return; }
	h = std::fmod(h, 360.f);
	if (h < 0.f) h += 360.f;

	float c = v * s;
	float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
	float m = v - c;
	float r1 = 0.f, g1 = 0.f, b1 = 0.f;

	if (h < 60.f)       { r1 = c; g1 = x; }
	else if (h < 120.f) { r1 = x; g1 = c; }
	else if (h < 180.f) { g1 = c; b1 = x; }
	else if (h < 240.f) { g1 = x; b1 = c; }
	else if (h < 300.f) { r1 = x; b1 = c; }
	else                { r1 = c; b1 = x; }

	r = r1 + m;
	g = g1 + m;
	b = b1 + m;
}

// ─── HSL → RGB ──────────────────────────────────────────────────────
// H ∈ [0, 360), S ∈ [0, 1], L ∈ [0, 1]
// Output R, G, B ∈ [0, 1]
inline void hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
	h = std::fmod(h, 360.f);
	if (h < 0.f) h += 360.f;

	float c = (1.f - std::fabs(2.f * l - 1.f)) * s;
	float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
	float m = l - c / 2.f;
	float r1 = 0.f, g1 = 0.f, b1 = 0.f;

	if (h < 60.f)       { r1 = c; g1 = x; }
	else if (h < 120.f) { r1 = x; g1 = c; }
	else if (h < 180.f) { g1 = c; b1 = x; }
	else if (h < 240.f) { g1 = x; b1 = c; }
	else if (h < 300.f) { r1 = x; b1 = c; }
	else                { r1 = c; b1 = x; }

	r = r1 + m;
	g = g1 + m;
	b = b1 + m;
}

// ─── Clamp helper (C++11 compatible) ────────────────────────────────
inline float clampf(float x, float lo, float hi) {
	return (x < lo) ? lo : (x > hi) ? hi : x;
}

// ─── OKLCH → Linear RGB ────────────────────────────────────────────
// L ∈ [0, 1], C ∈ [0, ~0.4], h ∈ [0, 360) degrees
// Output R, G, B ∈ [0, 1] (linear, NOT sRGB gamma — correct for laser DACs)
//
// Pipeline: OKLCH → Oklab → LMS (cube root) → LMS → linear sRGB
inline void oklchToRgb(float L, float C, float h_deg, float& r, float& g, float& b) {
	// OKLCH → Oklab
	float h_rad = h_deg * (float)M_PI / 180.f;
	float a = C * std::cos(h_rad);
	float bk = C * std::sin(h_rad);

	// Oklab → LMS (cube-root domain)
	float l_ = L + 0.3963377774f * a + 0.2158037573f * bk;
	float m_ = L - 0.1055613458f * a - 0.0638541728f * bk;
	float s_ = L - 0.0894841775f * a - 1.2914855480f * bk;

	// Undo cube root
	float l = l_ * l_ * l_;
	float m = m_ * m_ * m_;
	float s = s_ * s_ * s_;

	// LMS → linear sRGB
	r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
	g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
	b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

	// Clamp to [0, 1] — out-of-gamut OKLCH values may produce negatives
	r = clampf(r, 0.f, 1.f);
	g = clampf(g, 0.f, 1.f);
	b = clampf(b, 0.f, 1.f);
}

} // namespace color
} // namespace garmire
