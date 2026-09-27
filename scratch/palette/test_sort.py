import math

def hex_to_rgb(hex_str):
    hex_str = hex_str.lstrip('#')
    return tuple(int(hex_str[i:i+2], 16) for i in (0, 2, 4))

def rgb_to_lab(r, g, b):
    def to_lin(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    rl, gl, bl = to_lin(r), to_lin(g), to_lin(b)
    x = rl * 0.4124564 + gl * 0.3575761 + bl * 0.1804375
    y = rl * 0.2126729 + gl * 0.7151522 + bl * 0.0721750
    z = rl * 0.0193339 + gl * 0.1191920 + bl * 0.9503041
    x, y, z = x * 100, y * 100, z * 100
    xn, yn, zn = 95.047, 100.0, 108.883
    def f(t):
        return t ** (1/3) if t > 0.008856 else (7.787 * t) + (16 / 116)
    fx, fy, fz = f(x / xn), f(y / yn), f(z / zn)
    return (116 * fy) - 16, 500 * (fx - fy), 200 * (fy - fz)

def delta_e(c1, c2):
    return math.sqrt((c1[0]-c2[0])**2 + (c1[1]-c2[1])**2 + (c1[2]-c2[2])**2)

def simulate_deutan(r, g, b):
    return (min(255, max(0, int(0.625 * r + 0.375 * g))),
            min(255, max(0, int(0.700 * r + 0.300 * g))),
            min(255, max(0, int(0.300 * g + 0.700 * b))))

def simulate_protan(r, g, b):
    return (min(255, max(0, int(0.567 * r + 0.433 * g))),
            min(255, max(0, int(0.558 * r + 0.442 * g))),
            min(255, max(0, int(0.242 * g + 0.758 * b))))

def simulate_tritan(r, g, b):
    return (min(255, max(0, int(0.950 * r + 0.050 * g))),
            min(255, max(0, int(0.000 * r + 0.433 * g + 0.567 * b))),
            min(255, max(0, int(0.000 * r + 0.475 * g + 0.525 * b))))

palette = [
    ('White', '#FFFFFF'),
    ('Electric Yellow', '#F0E442'),
    ('Safety Orange', '#E69F00'),
    ('Coral Peach', '#FF8866'),
    ('Pastel Rose', '#EE99AA'),
    ('Pale Lavender', '#B8A0E8'),
    ('Sky Blue', '#56B4E9'),
    ('Bluish Green', '#009E73'),
    ('Forest Pine', '#117733'),
    ('Dark Petrol', '#005566'),
    ('Cobalt Blue', '#0072B2'),
    ('Imperial Violet', '#442288'),
    ('Deep Wine', '#882255'),
    ('Vermilion', '#D55E00'),
    ('Reddish Purple', '#CC79A7'),
    ('Charcoal', '#1A1A1A'),
]

def order_palette(items, sim_fn):
    n = len(items)
    labs = [rgb_to_lab(*sim_fn(*hex_to_rgb(c[1]))) for c in items]
    dist = [[delta_e(labs[i], labs[j]) for j in range(n)] for i in range(n)]

    best_path = None
    best_cost = 1e9

    for start in range(n):
        unvisited = set(range(n))
        unvisited.remove(start)
        path = [start]
        curr = start
        while unvisited:
            nxt = min(unvisited, key=lambda x: dist[curr][x])
            path.append(nxt)
            unvisited.remove(nxt)
            curr = nxt

        # 2-opt
        for _ in range(50):
            improved = False
            for i in range(n - 1):
                for j in range(i + 2, n):
                    old_sub = sum(dist[path[k]][path[k+1]] for k in range(i, j))
                    new_sub_path = path[i:j]
                    new_sub_path[1:] = reversed(new_sub_path[1:])
                    new_sub = sum(dist[new_sub_path[k]][new_sub_path[k+1]] for k in range(len(new_sub_path)-1))
                    if new_sub < old_sub - 1e-4:
                        path[i+1:j] = reversed(path[i+1:j])
                        improved = True
            if not improved:
                break

        cost = sum(dist[path[k]][path[k+1]] for k in range(n-1))
        if cost < best_cost:
            best_cost = cost
            best_path = path

    # Orient from lightest to darkest (White near top, Charcoal near bottom)
    white_idx = next(i for i, c in enumerate(items) if c[0] == 'White')
    charcoal_idx = next(i for i, c in enumerate(items) if c[0] == 'Charcoal')
    if best_path.index(white_idx) > best_path.index(charcoal_idx):
        best_path.reverse()

    return [(items[i][0], items[i][1], sim_fn(*hex_to_rgb(items[i][1]))) for i in best_path]

for name, fn in [
    ('NORMAL', lambda r, g, b: (r, g, b)),
    ('DEUTAN', simulate_deutan),
    ('PROTAN', simulate_protan),
    ('TRITAN', simulate_tritan),
]:
    res = order_palette(palette, fn)
    print(f'=== {name} ===')
    for item in res:
        print(f'  {item[0]:18} {item[1]} -> rgb{item[2]}')
