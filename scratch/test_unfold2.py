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

fig, axes = plt.subplots(1, 3, figsize=(15, 5))

# Test 1: Harmonic Angular Phase Fold
Nx, Ny = 3.0, 3.0
th_x = theta + 0.3 * np.sin(theta * Nx)
th_y = theta + 0.3 * np.sin(theta * Ny)
out_x = r * np.cos(th_x)
out_y = r * np.sin(th_y)
axes[0].plot(out_x, out_y, 'g-')
axes[0].set_title(f"Harmonic Angular Nx={Nx}")
axes[0].set_aspect('equal')

# Test 2: Nx=8, Ny=8
Nx, Ny = 8.0, 8.0
th_x = theta + 0.4 * np.sin(theta * Nx)
th_y = theta + 0.4 * np.sin(theta * Ny)
out_x = r * np.cos(th_x)
out_y = r * np.sin(th_y)
axes[1].plot(out_x, out_y, 'g-')
axes[1].set_title(f"Harmonic Angular Nx={Nx}")
axes[1].set_aspect('equal')

# Test 3: Bipolar Segment Unfold
# Continuous sector multiplication with sign flip
Nx, Ny = 5.0, 5.0
th_x = np.sign(Nx) * (std_th := theta * np.abs(Nx))
th_y = np.sign(Ny) * std_th
# wrap theta into [-pi, pi]
th_x = np.arctan2(np.sin(th_x), np.cos(th_x))
th_y = np.arctan2(np.sin(th_y), np.cos(th_y))
out_x = r * np.cos(th_x)
out_y = r * np.sin(th_y)
axes[2].plot(out_x, out_y, 'g-')
axes[2].set_title(f"Sector Wrap Nx={Nx}")
axes[2].set_aspect('equal')

plt.tight_layout()
plt.savefig("scratch/unfold_test2.png")
print("Saved unfold_test2.png")
