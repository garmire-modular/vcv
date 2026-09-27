import numpy as np
import matplotlib.pyplot as plt

def generate_cube(n_points=2000):
    t = np.linspace(0, 4, n_points)
    x = np.zeros_like(t)
    y = np.zeros_like(t)
    
    mask1 = (t >= 0) & (t < 1)
    x[mask1] = -1 + 2 * t[mask1]
    y[mask1] = -1
    
    mask2 = (t >= 1) & (t < 2)
    x[mask2] = 1
    y[mask2] = -1 + 2 * (t[mask2] - 1)
    
    mask3 = (t >= 2) & (t < 3)
    x[mask3] = 1 - 2 * (t[mask3] - 2)
    y[mask3] = 1
    
    mask4 = (t >= 3) & (t <= 4)
    x[mask4] = -1
    y[mask4] = 1 - 2 * (t[mask4] - 3)
    
    return x, y

x, y = generate_cube()
r = np.sqrt(x**2 + y**2)
theta = np.arctan2(y, x)

fig, axes = plt.subplots(2, 3, figsize=(15, 10))

# Candidate 1: Angular Sine Modulation
# theta_out = theta + depth * sin(N * theta)
# r_out = r
N = 6.0
A = 0.5
th1 = theta + A * np.sin(N * theta)
x1, y1 = r * np.cos(th1), r * np.sin(th1)
axes[0, 0].plot(x1, y1, 'g-')
axes[0, 0].set_title("1. Angular Phase Modulation")
axes[0, 0].set_aspect('equal')

# Candidate 2: Radial Fold / Waveshaper
# r_out = r * (1 + A * sin(N * theta))
r2 = r * (1 + 0.3 * np.sin(N * theta))
x2, y2 = r2 * np.cos(theta), r2 * np.sin(theta)
axes[0, 1].plot(x2, y2, 'g-')
axes[0, 1].set_title("2. Radial Modulation r*(1+A*sin(N*theta))")
axes[0, 1].set_aspect('equal')

# Candidate 3: Angular Triangle Folding
# theta_fold = triangle_wave(theta * N)
th3 = np.arcsin(np.sin(N * theta))
x3, y3 = r * np.cos(th3), r * np.sin(th3)
axes[0, 2].plot(x3, y3, 'g-')
axes[0, 2].set_title("3. Angular Triangle Fold asin(sin(N*theta))")
axes[0, 2].set_aspect('equal')

# Candidate 4: Combined Radial + Angular Folding (Rosette)
th4 = theta + 0.3 * np.sin(N * theta)
r4 = r * (1 + 0.25 * np.cos(N * theta))
x4, y4 = r4 * np.cos(th4), r4 * np.sin(th4)
axes[1, 0].plot(x4, y4, 'g-')
axes[1, 0].set_title("4. Combined Radial + Angular")
axes[1, 0].set_aspect('equal')

# Candidate 5: Multi-segment Sector Repeat (Modulus Angle)
# theta_out = (theta * N) % (2*pi) - pi
th5 = np.mod(theta * N + np.pi, 2 * np.pi) - np.pi
x5, y5 = r * np.cos(th5), r * np.sin(th5)
axes[1, 1].plot(x5, y5, 'g-')
axes[1, 1].set_title("5. Angle Modulo (theta * N % 2pi)")
axes[1, 1].set_aspect('equal')

# Candidate 6: Angular Wavefold with Sawtooth / Absolute Bounce
th6 = np.abs(np.mod(theta * N + np.pi, 2 * np.pi) - np.pi) - np.pi/2
x6, y6 = r * np.cos(th6), r * np.sin(th6)
axes[1, 2].plot(x6, y6, 'g-')
axes[1, 2].set_title("6. Mirrored Sector Fold")
axes[1, 2].set_aspect('equal')

plt.tight_layout()
plt.savefig("scratch/unfold_candidates.png")
print("Saved candidates plot.")
