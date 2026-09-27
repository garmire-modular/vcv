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

# Test A: Nx = 1.0, Ny = 3.0
Nx, Ny = 1.0, 3.0
th_x = theta + (Nx - 1.0) * np.sin(theta)
th_y = theta + (Ny - 1.0) * np.sin(theta)
out_x = r * np.cos(th_x)
out_y = r * np.sin(th_y)
axes[0].plot(out_x, out_y, 'g-')
axes[0].set_title(f"Nx={Nx}, Ny={Ny}")
axes[0].set_aspect('equal')

# Test B: Nx = 4.0, Ny = 4.0
Nx, Ny = 4.0, 4.0
th_x = theta + (Nx - 1.0) * np.sin(theta)
th_y = theta + (Ny - 1.0) * np.sin(theta)
out_x = r * np.cos(th_x)
out_y = r * np.sin(th_y)
axes[1].plot(out_x, out_y, 'g-')
axes[1].set_title(f"Nx={Nx}, Ny={Ny}")
axes[1].set_aspect('equal')

# Test C: Nx = 8.0, Ny = 8.0
Nx, Ny = 8.0, 8.0
th_x = theta + (Nx - 1.0) * np.sin(theta)
th_y = theta + (Ny - 1.0) * np.sin(theta)
out_x = r * np.cos(th_x)
out_y = r * np.sin(th_y)
axes[2].plot(out_x, out_y, 'g-')
axes[2].set_title(f"Nx={Nx}, Ny={Ny}")
axes[2].set_aspect('equal')

plt.tight_layout()
plt.savefig("scratch/unfold_phase_mod.png")
print("Saved unfold_phase_mod.png")
