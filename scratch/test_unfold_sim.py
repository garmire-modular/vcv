import numpy as np
import matplotlib.pyplot as plt

def generate_cube_points(n_points=1000):
    # Square/cube in 2D
    t = np.linspace(0, 4, n_points)
    x = np.zeros_like(t)
    y = np.zeros_like(t)
    
    # Side 1: (-1, -1) to (1, -1)
    mask1 = (t >= 0) & (t < 1)
    x[mask1] = -1 + 2 * t[mask1]
    y[mask1] = -1
    
    # Side 2: (1, -1) to (1, 1)
    mask2 = (t >= 1) & (t < 2)
    x[mask2] = 1
    y[mask2] = -1 + 2 * (t[mask2] - 1)
    
    # Side 3: (1, 1) to (-1, 1)
    mask3 = (t >= 2) & (t < 3)
    x[mask3] = 1 - 2 * (t[mask3] - 2)
    y[mask3] = 1
    
    # Side 4: (-1, 1) to (-1, -1)
    mask4 = (t >= 3) & (t <= 4)
    x[mask4] = -1
    y[mask4] = 1 - 2 * (t[mask4] - 3)
    
    return x, y

def unfold_transform(x, y, Nx, Ny):
    r = np.sqrt(x**2 + y**2)
    theta = np.arctan2(y, x)
    
    # Ping-pong / triangle fold of angle theta scaled by N
    # asin(sin(theta * N)) creates a smooth continuous ping-pong triangle wave between -pi/2 and pi/2
    theta_x = (2.0 / np.pi) * np.arcsin(np.sin(theta * Nx * 0.5)) * np.pi
    theta_y = (2.0 / np.pi) * np.arcsin(np.sin(theta * Ny * 0.5)) * np.pi
    
    out_x = r * np.cos(theta_x)
    out_y = r * np.sin(theta_y)
    return out_x, out_y

x, y = generate_cube_points()
fig, axes = plt.subplots(1, 3, figsize=(12, 4))

# Original Cube
axes[0].plot(x, y, 'g-')
axes[0].set_title("Original Cube")
axes[0].set_aspect('equal')

# Unfold N=3
ux3, uy3 = unfold_transform(x, y, 3.0, 3.0)
axes[1].plot(ux3, uy3, 'g-')
axes[1].set_title("Unfold N=3")
axes[1].set_aspect('equal')

# Unfold N=8
ux8, uy8 = unfold_transform(x, y, 8.0, 8.0)
axes[2].plot(ux8, uy8, 'g-')
axes[2].set_title("Unfold N=8")
axes[2].set_aspect('equal')

plt.tight_layout()
plt.savefig("scratch/unfold_sim.png")
print("Saved scratch/unfold_sim.png")
