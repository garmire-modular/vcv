#pragma once
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace eris {

template <typename T>
inline T clampVal(T v, T lo, T hi) {
	if (!std::isfinite(v)) return lo;
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

enum ModelId {
	MODEL_DRIFT = 0,
	MODEL_LORENZ,
	MODEL_ROSSLER,
	MODEL_BOUALI,
	MODEL_THOMAS,
	MODEL_AIZAWA,
	MODEL_CHEN,
	MODEL_MODIFIED_CHEN,
	MODEL_HALVORSEN,
	MODEL_NOSE_HOOVER,
	MODEL_SPROTT,
	MODEL_FOUR_WING,
	MODEL_PWL_DUFFING,
	MODEL_ARNEODO,
	MODEL_DEQUAN_LI,
	MODEL_RABINOVICH,
	MODEL_RIKITAKE,
	MODEL_DADRAS,
	NUM_MODELS
};

struct ModelMeta {
	const char* name;
	float defaultDt;
	int projX; // 0=X, 1=Y, 2=Z
	int projY; // 0=X, 1=Y, 2=Z
	int projZ; // 0=X, 1=Y, 2=Z
	float speedScale;
};

static const ModelMeta METADATA[NUM_MODELS] = {
	{ "Stochastic Drift",             0.020f, 0, 1, 2, 1.00f },
	{ "Lorenz Attractor",             0.015f, 0, 2, 1, 0.22f },
	{ "Rössler Attractor",            0.035f, 0, 1, 2, 1.00f },
	{ "Bouali Attractor",             0.020f, 0, 1, 2, 0.30f },
	{ "Thomas Symmetric",             0.080f, 0, 1, 2, 1.00f },
	{ "Aizawa Attractor",             0.020f, 0, 1, 2, 0.45f },
	{ "Chen Attractor",               0.010f, 0, 2, 1, 0.20f },
	{ "Modified Chen Attractor",      0.005f, 0, 1, 2, 0.25f },
	{ "Halvorsen Attractor",          0.015f, 0, 1, 2, 0.30f },
	{ "Nosé-Hoover Attractor",        0.030f, 0, 1, 2, 1.00f },
	{ "Sprott Attractor",             0.030f, 0, 1, 2, 1.00f },
	{ "Four-Wing Attractor",          0.040f, 0, 1, 2, 1.00f },
	{ "PWL Duffing Attractor",        0.020f, 0, 1, 2, 0.40f },
	{ "Arneodo Attractor",            0.030f, 0, 1, 2, 1.00f },
	{ "Dequan Li Five-Wing",          0.005f, 0, 1, 2, 1.00f },
	{ "Rabinovich-Fabrikant",         0.015f, 0, 1, 2, 1.00f },
	{ "Rikitake Dynamo",              0.020f, 0, 1, 2, 1.00f },
	{ "Dadras Attractor",             0.010f, 0, 1, 2, 1.00f }
};

const int ORBIT_POINTS = 1024;
const int WARMUP_STEPS = 300;

struct OrbitPoint {
	float x;
	float y;
	float z;
};

struct OrbitBuffer {
	OrbitPoint pts[ORBIT_POINTS];
	int model = -1;
	int seed = -1;
	float complexity = -1.f;
};

inline void getAttractorDerivatives(int model, const float* s, float* ds, float c, float zDelay = 0.f, float t = 0.f) {
	float x = s[0];
	float y = s[1];
	float z = s[2];

	switch (model) {
		case MODEL_LORENZ: {
			float sigma = 10.f;
			float beta = 8.f / 3.f;
			float rho = 14.f + 31.f * c;
			ds[0] = sigma * (y - x);
			ds[1] = x * (rho - z) - y;
			ds[2] = x * y - beta * z;
			break;
		}
		case MODEL_ROSSLER: {
			float a = 0.2f;
			float b = 0.2f;
			float paramC = 2.5f + 6.0f * c;
			ds[0] = -y - z;
			ds[1] = x + a * y;
			ds[2] = b + z * (x - paramC);
			break;
		}
		case MODEL_BOUALI: {
			float alpha = 3.0f;
			float beta = 1.0f;
			float mu = 0.001f;
			float gamma = 1.0f + 2.5f * c;
			ds[0] = alpha * x * (1.0f - y) - beta * z;
			ds[1] = -gamma * y * (1.0f - x * x);
			ds[2] = mu * x;
			break;
		}
		case MODEL_THOMAS: {
			float b = 0.32f - 0.14f * c;
			ds[0] = std::sin(y) - b * x;
			ds[1] = std::sin(z) - b * y;
			ds[2] = std::sin(x) - b * z;
			break;
		}
		case MODEL_AIZAWA: {
			float a = 0.95f;
			float b = 0.7f;
			float paramC = 0.6f;
			float e = 0.25f;
			float d = 3.5f + 1.5f * c;
			float f = 0.1f + 0.15f * c;
			ds[0] = (z - b) * x - d * y;
			ds[1] = d * x + (z - b) * y;
			ds[2] = paramC + a * z - (z * z * z) / 3.0f - (x * x + y * y) * (1.0f + e * z) + f * z * (x * x * x);
			break;
		}
		case MODEL_CHEN: {
			float a = 35.0f;
			float b = 3.0f;
			float paramC = 20.0f + 18.0f * c;
			ds[0] = a * (y - x);
			ds[1] = (paramC - a) * x - x * z + paramC * y;
			ds[2] = x * y - b * z;
			break;
		}
		case MODEL_MODIFIED_CHEN: {
			// Modified Chen system with time-delay feedback:
			// dx/dt = a*(y - x)
			// dy/dt = (c - a)*x - x*f + c*y
			// dz/dt = x*y - b*z
			// f = d0*z + d1*z(t - tau) - d2*sin(z(t - tau))
			// Standard parameters: a = 35, c = 28, b = 3, d0 = 1, d1 = 1, d2 in [-20, 20]
			float a = 35.0f;
			float paramC = 28.0f;
			float b = 3.0f;
			float d0 = 1.0f;
			float d1 = 1.0f;
			// Complexity parameter modulates d2 across [-20, 20]
			float d2 = -20.0f + 40.0f * c;
			float f = d0 * z + d1 * zDelay - d2 * std::sin(zDelay);
			ds[0] = a * (y - x);
			ds[1] = (paramC - a) * x - x * f + paramC * y;
			ds[2] = x * y - b * z;
			break;
		}
		case MODEL_HALVORSEN: {
			float a = 1.2f + 1.3f * c;
			ds[0] = -a * x - 4.0f * y - 4.0f * z - y * y;
			ds[1] = -a * y - 4.0f * z - 4.0f * x - z * z;
			ds[2] = -a * z - 4.0f * x - 4.0f * y - x * x;
			break;
		}
		case MODEL_NOSE_HOOVER: {
			float alpha = 1.0f + 1.5f * c;
			ds[0] = y;
			ds[1] = -x + y * z;
			ds[2] = alpha - y * y;
			break;
		}
		case MODEL_SPROTT: {
			float paramC = 0.8f + 0.6f * c;
			ds[0] = y;
			ds[1] = -x + paramC * y * z;
			ds[2] = 1.0f - y * y;
			break;
		}
		case MODEL_FOUR_WING: {
			float a = 0.2f;
			float b = 0.01f;
			float paramC = -0.4f;
			float d = -1.0f;
			float e = 0.6f + 1.4f * c;
			ds[0] = a * x + b * y - y * z;
			ds[1] = paramC * y + x * z;
			ds[2] = d * z - x * y + e;
			break;
		}
		case MODEL_PWL_DUFFING: {
			// Piecewise-linear Duffing system:
			// dx/dt = y
			// dy/dt = -m1*x - 0.5*(m0 - m1)*(|x+1| - |x-1|) - e*y + gamma*cos(omega*t)
			// e = 0.25, m0 = -0.0845, m1 = 0.66, omega = 1.0 rad/s
			// gamma in [-1.11, 1.39] modulated by complexity c
			float e = 0.25f;
			float m0 = -0.0845f;
			float m1 = 0.66f;
			float omega = 1.0f;
			float gamma = -1.11f + 2.50f * c;
			float pwl = -m1 * x - 0.5f * (m0 - m1) * (std::abs(x + 1.0f) - std::abs(x - 1.0f));
			ds[0] = y;
			ds[1] = pwl - e * y + gamma * std::cos(omega * t);
			// Z is synthetic quadrature phase for 3D trajectory
			ds[2] = -omega * x;
			break;
		}
		case MODEL_ARNEODO: {
			float a = 5.5f;
			float b = 3.5f;
			float paramC = 0.4f + 1.4f * c;
			ds[0] = y;
			ds[1] = z;
			ds[2] = -a * x - b * y - z + paramC * (x * x * x);
			break;
		}
		case MODEL_DEQUAN_LI: {
			float a = 40.0f;
			float b = 1.833f;
			float paramC = 55.0f;
			float d = 0.16f;
			float f = 20.0f;
			float e = 0.5f + 0.35f * c;
			ds[0] = a * (y - x) + d * x * z;
			ds[1] = paramC * x + f * y - x * z;
			ds[2] = b * z + x * y - e * (x * x);
			break;
		}
		case MODEL_RABINOVICH: {
			float gamma = 0.14f;
			float alpha = 0.05f + 0.20f * c;
			ds[0] = y * (z - 1.0f + x * x) + gamma * x;
			ds[1] = x * (3.0f * z + 1.0f - x * x) + gamma * y;
			ds[2] = -2.0f * z * (alpha + x * y);
			break;
		}
		case MODEL_RIKITAKE: {
			float mu = 1.0f;
			float a = 1.2f + 2.3f * c;
			ds[0] = -mu * x + y * z;
			ds[1] = -mu * y + (z - a) * x;
			ds[2] = 1.0f - x * y;
			break;
		}
		case MODEL_DADRAS: {
			float a = 3.0f;
			float paramC = 1.7f;
			float d = 2.0f;
			float e = 9.0f;
			float b = 1.5f + 2.3f * c;
			ds[0] = y - a * x + b * y * z;
			ds[1] = paramC * y - x * z + z;
			ds[2] = d * x * y - e * z;
			break;
		}
		default:
			ds[0] = 0.f;
			ds[1] = 0.f;
			ds[2] = 0.f;
			break;
	}

	// Watchdog clamp on derivatives
	ds[0] = clampVal(ds[0], -500.f, 500.f);
	ds[1] = clampVal(ds[1], -500.f, 500.f);
	ds[2] = clampVal(ds[2], -500.f, 500.f);
}

inline void stepRK4(int model, float* s, float dt, float complexity, float zDelay = 0.f, float t = 0.f) {
	float k1[3], k2[3], k3[3], k4[3];
	float temp[3];

	getAttractorDerivatives(model, s, k1, complexity, zDelay, t);

	temp[0] = s[0] + 0.5f * dt * k1[0];
	temp[1] = s[1] + 0.5f * dt * k1[1];
	temp[2] = s[2] + 0.5f * dt * k1[2];
	getAttractorDerivatives(model, temp, k2, complexity, zDelay, t + 0.5f * dt);

	temp[0] = s[0] + 0.5f * dt * k2[0];
	temp[1] = s[1] + 0.5f * dt * k2[1];
	temp[2] = s[2] + 0.5f * dt * k2[2];
	getAttractorDerivatives(model, temp, k3, complexity, zDelay, t + 0.5f * dt);

	temp[0] = s[0] + dt * k3[0];
	temp[1] = s[1] + dt * k3[1];
	temp[2] = s[2] + dt * k3[2];
	getAttractorDerivatives(model, temp, k4, complexity, zDelay, t + dt);

	s[0] += (dt / 6.0f) * (k1[0] + 2.0f * k2[0] + 2.0f * k3[0] + k4[0]);
	s[1] += (dt / 6.0f) * (k1[1] + 2.0f * k2[1] + 2.0f * k3[1] + k4[1]);
	s[2] += (dt / 6.0f) * (k1[2] + 2.0f * k2[2] + 2.0f * k3[2] + k4[2]);

	s[0] = clampVal(s[0], -1000.f, 1000.f);
	s[1] = clampVal(s[1], -1000.f, 1000.f);
	s[2] = clampVal(s[2], -1000.f, 1000.f);
}

inline void generateOrbit(OrbitBuffer& buf, int model, int seed, float complexity) {
	buf.model = model;
	buf.seed = seed;
	buf.complexity = complexity;

	if (model == MODEL_DRIFT) {
		// Stochastic Drift: Multi-harmonic Fourier summation
		static const float freqs[8] = { 1.0f, 2.718f, 4.14f, 7.38f, 11.0f, 15.6f, 21.2f, 28.5f };
		static const float amps[8]  = { 1.0f, 0.65f,  0.45f, 0.32f, 0.22f,  0.15f,  0.10f,  0.07f };
		int K = 1 + (int)(clampVal(complexity, 0.0f, 1.0f) * 7.0f);
		if (K > 8) K = 8;
		float sumA = 0.f;
		for (int i = 0; i < K; ++i) sumA += amps[i];
		if (sumA < 1e-5f) sumA = 1.f;

		float seedOffset = (float)seed * 1.618033f;

		for (int n = 0; n < ORBIT_POINTS; ++n) {
			float phi = (float)n / (float)ORBIT_POINTS;
			float phi0 = 2.0f * (float)M_PI * phi;
			float x = 0.f;
			float y = 0.f;

			for (int i = 0; i < K; ++i) {
				float theta_i = phi0 * freqs[i] + seedOffset + (float)i * 2.39996f;
				x += amps[i] * std::sin(theta_i);
				y += amps[i] * std::cos(1.618f * theta_i + 1.3f);
			}

			float finalX = (x / sumA) * 0.95f;
			float finalY = (y / sumA) * 0.95f;
			float dist = std::sqrt(finalX * finalX + finalY * finalY);
			float finalZ = clampVal(dist / 1.35f, 0.0f, 1.0f);

			buf.pts[n].x = clampVal(finalX, -1.0f, 1.0f);
			buf.pts[n].y = clampVal(finalY, -1.0f, 1.0f);
			buf.pts[n].z = clampVal(finalZ, 0.0f, 1.0f);
		}
		return;
	}

	// Strange Attractor Models (RK4 integration)
	float s[3] = { 0.1f, 0.1f, 0.1f };
	if (model == MODEL_CHEN) { s[0] = -0.1f; s[1] = 0.5f; s[2] = -0.6f; }
	else if (model == MODEL_MODIFIED_CHEN) { s[0] = 1.0f; s[1] = 1.0f; s[2] = 14.0f; }
	else if (model == MODEL_HALVORSEN) { s[0] = -1.48f; s[1] = -1.2f; s[2] = 0.5f; }
	else if (model == MODEL_BOUALI) { s[0] = 1.0f; s[1] = 0.1f; s[2] = 0.1f; }
	else if (model == MODEL_PWL_DUFFING) { s[0] = 0.1f; s[1] = 0.1f; s[2] = 0.1f; }
	else if (model == MODEL_RABINOVICH) { s[0] = -1.0f; s[1] = 0.0f; s[2] = 0.5f; }
	else if (model == MODEL_DADRAS) { s[0] = 1.0f; s[1] = 1.0f; s[2] = 1.0f; }

	// Seed deterministic micro-perturbation
	float pX = std::sin((float)seed * 12.9898f) * 0.02f;
	float pY = std::cos((float)seed * 78.2330f) * 0.02f;
	float pZ = std::sin((float)seed * 45.1640f) * 0.02f;
	s[0] += pX;
	s[1] += pY;
	s[2] += pZ;

	float dt = METADATA[model].defaultDt;

	// Delay buffer for Modified Chen (tau = 0.2s, dt = 0.005s => 40 delay steps)
	const int DELAY_STEPS = 40;
	float zRing[64];
	for (int i = 0; i < 64; ++i) zRing[i] = s[2];
	int ringIdx = 0;
	float t = 0.f;

	// Warmup steps to drop onto the strange attractor manifold
	for (int step = 0; step < WARMUP_STEPS; ++step) {
		float zDel = zRing[(ringIdx - DELAY_STEPS + 64) % 64];
		stepRK4(model, s, dt, complexity, zDel, t);
		zRing[ringIdx] = s[2];
		ringIdx = (ringIdx + 1) % 64;
		t += dt;
	}

	float minCoord[3] = { 1e9f, 1e9f, 1e9f };
	float maxCoord[3] = { -1e9f, -1e9f, -1e9f };

	float rawTraj[ORBIT_POINTS][3];

	for (int n = 0; n < ORBIT_POINTS; ++n) {
		float zDel = zRing[(ringIdx - DELAY_STEPS + 64) % 64];
		stepRK4(model, s, dt, complexity, zDel, t);
		zRing[ringIdx] = s[2];
		ringIdx = (ringIdx + 1) % 64;
		t += dt;

		rawTraj[n][0] = s[0];
		rawTraj[n][1] = s[1];
		rawTraj[n][2] = s[2];

		for (int dim = 0; dim < 3; ++dim) {
			if (s[dim] < minCoord[dim]) minCoord[dim] = s[dim];
			if (s[dim] > maxCoord[dim]) maxCoord[dim] = s[dim];
		}
	}

	const ModelMeta& meta = METADATA[model];
	int px = meta.projX;
	int py = meta.projY;
	int pz = meta.projZ;

	float rangeX = maxCoord[px] - minCoord[px];
	float rangeY = maxCoord[py] - minCoord[py];
	float rangeZ = maxCoord[pz] - minCoord[pz];
	if (rangeX < 1e-4f) rangeX = 1e-4f;
	if (rangeY < 1e-4f) rangeY = 1e-4f;
	if (rangeZ < 1e-4f) rangeZ = 1e-4f;

	for (int n = 0; n < ORBIT_POINTS; ++n) {
		float rx = rawTraj[n][px];
		float ry = rawTraj[n][py];
		float rz = rawTraj[n][pz];

		float nx = (2.0f * (rx - minCoord[px]) / rangeX - 1.0f) * 0.95f;
		float ny = (2.0f * (ry - minCoord[py]) / rangeY - 1.0f) * 0.95f;
		float nz = (rz - minCoord[pz]) / rangeZ; // [0.0, 1.0]

		buf.pts[n].x = clampVal(nx, -1.0f, 1.0f);
		buf.pts[n].y = clampVal(ny, -1.0f, 1.0f);
		buf.pts[n].z = clampVal(nz, 0.0f, 1.0f);
	}
}

inline OrbitPoint sampleOrbit(const OrbitBuffer& buf, float phase) {
	float phi = phase - std::floor(phase);
	if (phi < 0.f) phi += 1.f;

	float idx = phi * (float)ORBIT_POINTS;
	int i0 = (int)idx % ORBIT_POINTS;
	int i1 = (i0 + 1) % ORBIT_POINTS;
	float alpha = idx - (float)(int)idx;

	OrbitPoint p;
	p.x = (1.0f - alpha) * buf.pts[i0].x + alpha * buf.pts[i1].x;
	p.y = (1.0f - alpha) * buf.pts[i0].y + alpha * buf.pts[i1].y;
	p.z = (1.0f - alpha) * buf.pts[i0].z + alpha * buf.pts[i1].z;
	return p;
}

} // namespace eris
