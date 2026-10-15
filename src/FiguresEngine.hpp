#pragma once
#include <cmath>
#include <algorithm>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace figures {

template <typename T>
inline T clampVal(T v, T lo, T hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

enum ModeId {
	MODE_FIXE = 0,
	MODE_TRANSLATION,
	MODE_ROTATION,
	MODE_ELARGISSEMENT,
	MODE_CONTOURNEMENT,
	MODE_ZIGZAG,
	MODE_ECHAPPEMENT,
	MODE_BALANCEMENT,
	MODE_SPIRALE,
	MODE_VAGUE,
	MODE_TRESSAGE,
	MODE_REBONDISSEMENT,
	MODE_DISPERSION,
	MODE_POLYPHONIE,
	MODE_COMBINEE,
	MODE_ENVELOPPEMENT,
	NUM_MODES
};

enum CombineeVariant {
	COMBINEE_DEPTH = 0,
	COMBINEE_DIRECTION,
	COMBINEE_PHASE_SHEAR
};

struct FigureMetadata {
	const char* name;
	const char* p1Name;
	float p1Min;
	float p1Max;
	float p1Def;
	const char* p1Unit;
	const char* p2Name;
	float p2Min;
	float p2Max;
	float p2Def;
	const char* p2Unit;
};

static const FigureMetadata METADATA[NUM_MODES] = {
	// Mode 0: Fixe
	{ "Fixe", "Micro-breathing", 0.01f, 0.35f, 0.15f, "", "Pulse rate", 1.0f, 8.0f, 3.0f, "x" },
	// Mode 1: Translation
	{ "Translation", "Angle", 0.0f, 360.0f, 0.0f, "°", "Corridor length", 0.20f, 1.00f, 0.85f, "" },
	// Mode 2: Rotation
	{ "Rotation", "Radius", 0.10f, 0.95f, 0.75f, "", "Turns / cycle", 1.0f, 4.0f, 1.0f, "x" },
	// Mode 3: Élargissement
	{ "Élargissement", "Min caliber", 0.05f, 0.40f, 0.15f, "", "Max caliber", 0.50f, 0.95f, 0.85f, "" },
	// Mode 4: Contournement
	{ "Contournement", "Proximity", 0.50f, 0.98f, 0.92f, "", "Undulation", 0.00f, 0.25f, 0.08f, "" },
	// Mode 5: Zig-zag
	{ "Zig-zag", "Cycles", 2.0f, 8.0f, 4.0f, "x", "Corridor width", 0.30f, 0.95f, 0.85f, "" },
	// Mode 6: Échappement
	{ "Échappement", "Spiral turns", 1.0f, 5.0f, 2.5f, "x", "Ejection", 0.50f, 4.00f, 1.40f, "" },
	// Mode 7: Balancement
	{ "Balancement", "Swing amplitude", 0.30f, 0.95f, 0.88f, "", "Pendulum arc", 0.00f, 0.50f, 0.25f, "" },
	// Mode 8: Spirale
	{ "Spirale", "Revolutions", 1.0f, 6.0f, 3.0f, "x", "Expansion ratio", 0.10f, 1.00f, 0.80f, "" },
	// Mode 9: Vague
	{ "Vague", "Wave frequency", 1.0f, 6.0f, 3.0f, "x", "Wave height", 0.10f, 0.80f, 0.45f, "" },
	// Mode 10: Tressage
	{ "Tressage", "Braid crossings", 2.0f, 8.0f, 4.0f, "x", "Intertwine width", 0.10f, 0.60f, 0.35f, "" },
	// Mode 11: Rebondissement
	{ "Rebondissement", "Wall rebounds", 2.0f, 10.0f, 5.0f, "x", "Ballistic arc", 0.00f, 0.80f, 0.20f, "" },
	// Mode 12: Dispersion
	{ "Dispersion", "Burst count", 1.0f, 6.0f, 3.0f, "", "Spray cone turbulence", 0.00f, 1.00f, 0.30f, "" },
	// Mode 13: Polyphonie
	{ "Polyphonie spatiale", "Orbital radius", 0.10f, 0.50f, 0.35f, "", "Counterpoint ratio", 1.0f, 8.0f, 3.0f, "x" },
	// Mode 14: Combinée
	{ "Trajectoire combinée", "Tournoiement ratio", 1.0f, 6.0f, 3.0f, "x", "Variant parameter", 0.00f, 1.00f, 0.35f, "" },
	// Mode 15: Enveloppement
	{ "Enveloppement", "Expansion depth", 0.20f, 0.98f, 0.90f, "", "Cocoon lobes", 1.0f, 8.0f, 4.0f, "x" }
};

struct FiguresEngine {
	CombineeVariant combineeVariant = COMBINEE_DEPTH;

	std::pair<float, float> compute(int mode, float phase, float p1, float p2) const {
		// Normalize phase to [0.0, 1.0)
		float phi = phase - std::floor(phase);
		if (phi < 0.f) phi += 1.f;

		const float twoPi = (float)(2.0 * M_PI);
		const float pi = (float)M_PI;

		float x = 0.f;
		float y = 0.f;

		switch (mode) {
			case MODE_FIXE: {
				// Stationary Focal Point with micro-breathing
				float phi0 = twoPi * phi;
				x = p1 * std::sin(phi0 * p2);
				y = p1 * std::cos(phi0 * p2 * 1.3f);
				break;
			}
			case MODE_TRANSLATION: {
				// Linear Acoustic Corridor
				float thetaRad = p1 * (pi / 180.f);
				float pos = p2 * std::cos(twoPi * phi);
				x = pos * std::cos(thetaRad);
				y = pos * std::sin(thetaRad);
				break;
			}
			case MODE_ROTATION: {
				// Orbital Revolution
				float theta = twoPi * phi * p2;
				x = p1 * std::cos(theta);
				y = p1 * std::sin(theta);
				break;
			}
			case MODE_ELARGISSEMENT: {
				// Spatial Dilation & Contraction
				float r = p1 + (p2 - p1) * (0.5f + 0.5f * std::sin(twoPi * phi));
				x = r * std::cos(4.f * pi * phi);
				y = r * std::sin(4.f * pi * phi);
				break;
			}
			case MODE_CONTOURNEMENT: {
				// Boundary Grazing & Bypassing
				float theta = twoPi * phi;
				float r = p1 + p2 * std::sin(8.f * pi * phi);
				x = r * std::cos(theta);
				y = r * std::sin(theta);
				break;
			}
			case MODE_ZIGZAG: {
				// Angular Vector Scans
				float u = std::fmod(phi * p1, 2.0f);
				if (u < 0.f) u += 2.0f;
				float tri = (std::abs(u - 1.0f) * 2.0f - 1.0f) * p2;
				x = tri;
				y = 0.75f * std::cos(twoPi * phi);
				break;
			}
			case MODE_ECHAPPEMENT: {
				// Centrifugal Escape Flight
				float p = phi;
				float r = (p > 0.f) ? (0.95f * std::pow(p, p2)) : 0.f;
				float theta = twoPi * p * p1;
				x = r * std::cos(theta);
				y = r * std::sin(theta);
				break;
			}
			case MODE_BALANCEMENT: {
				// Acoustic Pendulum
				float theta = twoPi * phi;
				x = p1 * std::sin(theta);
				y = -p2 * (1.0f - std::cos(2.0f * theta));
				break;
			}
			case MODE_SPIRALE: {
				// Centripetal / Centrifugal Vortex
				float p = phi;
				float r = 0.15f + 0.80f * p2 * (0.5f + 0.5f * std::sin(pi * p));
				float theta = twoPi * p * p1;
				x = r * std::cos(theta);
				y = r * std::sin(theta);
				break;
			}
			case MODE_VAGUE: {
				// Transverse Wave Motion
				float theta = twoPi * phi;
				x = 0.85f * std::cos(theta);
				y = p2 * std::sin(theta * p1);
				break;
			}
			case MODE_TRESSAGE: {
				// Dual Braided Counterpoint
				float theta = twoPi * phi;
				x = 0.80f * std::cos(theta);
				y = p2 * std::sin(theta * p1);
				break;
			}
			case MODE_REBONDISSEMENT: {
				// Billiard Ricochets with Ballistic Sag
				float u = std::fmod(phi * p1, 2.0f);
				if (u < 0.f) u += 2.0f;
				float w = std::fmod(phi * p1 * 0.7f + 0.4f, 2.0f);
				if (w < 0.f) w += 2.0f;
				float xRaw = (std::abs(u - 1.0f) * 2.0f - 1.0f) * 0.88f;
				float yRaw = (std::abs(w - 1.0f) * 2.0f - 1.0f) * 0.88f;
				float xNorm = xRaw / 0.88f;
				y = yRaw - p2 * (1.0f - xNorm * xNorm);
				x = xRaw;
				break;
			}
			case MODE_DISPERSION: {
				// Explosive Centrifugal Shards
				float N = clampVal(std::floor(p1), 1.0f, 6.0f);
				float burstIndex = std::floor(phi * N);
				float p = std::fmod(phi * N, 1.0f);
				if (p < 0.f) p += 1.0f;
				float thetaBurst = burstIndex * (twoPi / N);
				float r = (p > 0.f) ? (0.90f * std::pow(p, 0.7f)) : 0.f;
				float theta = thetaBurst + p2 * 0.5f * std::sin(4.f * pi * p);
				x = r * std::cos(theta);
				y = r * std::sin(theta);
				break;
			}
			case MODE_POLYPHONIE: {
				// Contrapuntal Macro/Micro Orbits
				float theta = twoPi * phi;
				float xCarrier = 0.50f * std::sin(0.5f * theta);
				float xSatellite = p1 * std::cos(theta * p2);
				float ySatellite = p1 * std::sin(theta * p2);
				x = xCarrier + xSatellite;
				y = ySatellite;
				break;
			}
			case MODE_COMBINEE: {
				// Composite Epicycloid / Trochoid
				float theta = twoPi * phi;
				if (combineeVariant == COMBINEE_DEPTH) {
					float d = clampVal(p2, 0.0f, 1.0f);
					x = (1.0f - 0.5f * d) * 0.55f * std::cos(theta) + 0.35f * d * std::cos(theta * p1);
					y = (1.0f - 0.5f * d) * 0.55f * std::sin(theta) + 0.35f * d * std::sin(theta * p1);
				} else if (combineeVariant == COMBINEE_DIRECTION) {
					float s = (p2 < 0.5f) ? -1.0f : 1.0f;
					x = 0.55f * std::cos(theta) + 0.35f * std::cos(theta * p1 * s);
					y = 0.55f * std::sin(theta) + 0.35f * std::sin(theta * p1 * s);
				} else {
					// COMBINEE_PHASE_SHEAR
					float psi = p2 * pi;
					x = 0.55f * std::cos(theta) + 0.35f * std::cos(theta * p1);
					y = 0.55f * std::sin(theta) + 0.35f * std::sin(theta * p1 + psi);
				}
				break;
			}
			case MODE_ENVELOPPEMENT: {
				// Omnidirectional Multi-Lobed Cocoon
				float theta = twoPi * phi;
				float r = p1 * (0.5f + 0.5f * std::sin(theta));
				float thetaCocoon = theta * p2;
				x = r * std::cos(thetaCocoon);
				y = r * std::sin(thetaCocoon);
				break;
			}
			default:
				break;
		}

		// Hard boundary clamping [-1.0, 1.0] per DSP guideline
		x = clampVal(x, -1.0f, 1.0f);
		y = clampVal(y, -1.0f, 1.0f);

		return { x, y };
	}
};

} // namespace figures
