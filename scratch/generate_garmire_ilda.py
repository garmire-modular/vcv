import colorsys
import math
import os
import re
import struct
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw


def parse_svg_numbers(s):
    return [float(x) for x in re.findall(r'[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?', s)]


def svg_arc_to_points(x1, y1, rx, ry, phi_deg, large_arc, sweep, x2, y2, min_steps=6):
    """Exact W3C SVG 1.1 F.6.5 / F.6.6 elliptical arc parameterization."""
    if math.hypot(x2 - x1, y2 - y1) < 1e-9:
        return [(x2, y2)]
    if rx <= 0 or ry <= 0:
        return [(x2, y2)]

    phi = math.radians(phi_deg % 360.0)
    cp, sp = math.cos(phi), math.sin(phi)
    dx2 = 0.5 * (x1 - x2)
    dy2 = 0.5 * (y1 - y2)
    x1p = cp * dx2 + sp * dy2
    y1p = -sp * dx2 + cp * dy2

    rx_sq = rx * rx
    ry_sq = ry * ry
    x1p_sq = x1p * x1p
    y1p_sq = y1p * y1p

    lam = x1p_sq / rx_sq + y1p_sq / ry_sq
    if lam > 1.0:
        scale = math.sqrt(lam)
        rx *= scale
        ry *= scale
        rx_sq = rx * rx
        ry_sq = ry * ry

    sign = -1.0 if (large_arc == sweep) else 1.0
    num = max(0.0, rx_sq * ry_sq - rx_sq * y1p_sq - ry_sq * x1p_sq)
    den = rx_sq * y1p_sq + ry_sq * x1p_sq
    coef = sign * math.sqrt(num / den) if den > 1e-12 else 0.0
    cxp = coef * (rx * y1p / ry)
    cyp = coef * -(ry * x1p / rx)

    cx = cp * cxp - sp * cyp + 0.5 * (x1 + x2)
    cy = sp * cxp + cp * cyp + 0.5 * (y1 + y2)

    def vec_angle(ux, uy, vx, vy):
        dot = ux * vx + uy * vy
        mag = math.hypot(ux, uy) * math.hypot(vx, vy)
        if mag < 1e-12:
            return 0.0
        val = max(-1.0, min(1.0, dot / mag))
        ang = math.acos(val)
        if ux * vy - uy * vx < 0:
            ang = -ang
        return ang

    theta1 = vec_angle(1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dtheta = vec_angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)

    if sweep == 0 and dtheta > 0:
        dtheta -= 2.0 * math.pi
    elif sweep == 1 and dtheta < 0:
        dtheta += 2.0 * math.pi

    arc_len = abs(dtheta) * 0.5 * (rx + ry)
    steps = max(min_steps, int(math.ceil(arc_len / 2.5)))
    pts = []
    for i in range(1, steps + 1):
        t = theta1 + dtheta * (i / steps)
        xt = rx * math.cos(t)
        yt = ry * math.sin(t)
        px = cp * xt - sp * yt + cx
        py = sp * xt + cp * yt + cy
        pts.append((px, py))
    return pts


def parse_svg_path_subpaths(d_str, tx=0.0, ty=0.0):
    """Parse SVG path d attribute into flattened polylines (without forced redraws)."""
    tokens = re.findall(r'[MmLlHhVvCcSsQqTtAaZz]|[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?', d_str)
    subpaths = []
    cur_sub = []
    cx = cy = sx = sy = 0.0
    prev_cpx = prev_cpy = 0.0
    prev_cmd = ''
    i = 0
    cmd = ''

    def next_f():
        nonlocal i
        val = float(tokens[i])
        i += 1
        return val

    def is_num():
        return i < len(tokens) and not re.match(r'^[MmLlHhVvCcSsQqTtAaZz]$', tokens[i])

    while i < len(tokens):
        if re.match(r'^[MmLlHhVvCcSsQqTtAaZz]$', tokens[i]):
            cmd = tokens[i]
            i += 1
        elif cmd == 'M':
            cmd = 'L'
        elif cmd == 'm':
            cmd = 'l'

        if cmd in ('M', 'm'):
            if len(cur_sub) >= 2:
                subpaths.append(cur_sub)
            x, y = next_f(), next_f()
            if cmd == 'm':
                x += cx
                y += cy
            cx, cy = x, y
            sx, sy = x, y
            cur_sub = [(cx + tx, cy + ty)]
            prev_cmd = cmd
            while is_num():
                x, y = next_f(), next_f()
                if cmd == 'm':
                    x += cx
                    y += cy
                cx, cy = x, y
                cur_sub.append((cx + tx, cy + ty))
                prev_cmd = 'L' if cmd == 'M' else 'l'

        elif cmd in ('L', 'l'):
            while is_num():
                x, y = next_f(), next_f()
                if cmd == 'l':
                    x += cx
                    y += cy
                cx, cy = x, y
                cur_sub.append((cx + tx, cy + ty))
                prev_cmd = cmd

        elif cmd in ('H', 'h'):
            while is_num():
                x = next_f()
                if cmd == 'h':
                    x += cx
                cx = x
                cur_sub.append((cx + tx, cy + ty))
                prev_cmd = cmd

        elif cmd in ('V', 'v'):
            while is_num():
                y = next_f()
                if cmd == 'v':
                    y += cy
                cy = y
                cur_sub.append((cx + tx, cy + ty))
                prev_cmd = cmd

        elif cmd in ('C', 'c'):
            while is_num():
                x1, y1, x2, y2, x3, y3 = next_f(), next_f(), next_f(), next_f(), next_f(), next_f()
                if cmd == 'c':
                    x1 += cx; y1 += cy
                    x2 += cx; y2 += cy
                    x3 += cx; y3 += cy
                poly_len = math.hypot(x1 - cx, y1 - cy) + math.hypot(x2 - x1, y2 - y1) + math.hypot(x3 - x2, y3 - y2)
                steps = max(2, min(16, int(math.ceil(poly_len / 3.0))))
                for s in range(1, steps + 1):
                    t = s / steps
                    u = 1.0 - t
                    bx = u**3 * cx + 3*u**2*t * x1 + 3*u*t**2 * x2 + t**3 * x3
                    by = u**3 * cy + 3*u**2*t * y1 + 3*u*t**2 * y2 + t**3 * y3
                    cur_sub.append((bx + tx, by + ty))
                prev_cpx, prev_cpy = x2, y2
                cx, cy = x3, y3
                prev_cmd = cmd

        elif cmd in ('A', 'a'):
            while is_num():
                rx, ry, phi = next_f(), next_f(), next_f()
                large_arc, sweep = int(next_f()), int(next_f())
                x2, y2 = next_f(), next_f()
                if cmd == 'a':
                    x2 += cx
                    y2 += cy
                arc_pts = svg_arc_to_points(cx, cy, rx, ry, phi, large_arc, sweep, x2, y2, min_steps=6)
                for px, py in arc_pts:
                    cur_sub.append((px + tx, py + ty))
                cx, cy = x2, y2
                prev_cmd = cmd

        elif cmd in ('Z', 'z'):
            if cur_sub and math.hypot(cur_sub[0][0] - (cx + tx), cur_sub[0][1] - (cy + ty)) > 1e-4:
                cur_sub.append(cur_sub[0])
            if len(cur_sub) >= 2:
                subpaths.append(cur_sub)
            cur_sub = []
            cx, cy = sx, sy
            prev_cmd = cmd
        else:
            break

    if len(cur_sub) >= 2:
        subpaths.append(cur_sub)
    return subpaths


def clip_polyline_to_circle(pts, ccx, ccy, rad):
    """Split an SVG polyline at exact intersections with circle (ccx, ccy, rad)."""
    chains = []
    cur = []

    def inside(x, y):
        return (x - ccx)**2 + (y - ccy)**2 <= (rad + 1e-3)**2

    for i in range(len(pts) - 1):
        x0, y0 = pts[i]
        x1, y1 = pts[i + 1]
        dx = x1 - x0
        dy = y1 - y0
        fx = x0 - ccx
        fy = y0 - ccy
        a = dx * dx + dy * dy
        if a < 1e-12:
            continue
        b = 2.0 * (fx * dx + fy * dy)
        c = fx * fx + fy * fy - rad * rad
        disc = b * b - 4.0 * a * c
        ts = [0.0, 1.0]
        if disc >= 0.0:
            sd = math.sqrt(disc)
            for t in ((-b - sd) / (2.0 * a), (-b + sd) / (2.0 * a)):
                if 1e-5 < t < 1.0 - 1e-5:
                    ts.append(t)
        ts.sort()
        for k in range(len(ts) - 1):
            t0, t1 = ts[k], ts[k + 1]
            if t1 - t0 < 1e-5:
                continue
            tm = 0.5 * (t0 + t1)
            mx = x0 + tm * dx
            my = y0 + tm * dy
            if inside(mx, my):
                p0 = (x0 + t0 * dx, y0 + t0 * dy)
                p1 = (x0 + t1 * dx, y0 + t1 * dy)
                if cur and math.hypot(cur[-1][0] - p0[0], cur[-1][1] - p0[1]) < 1e-2:
                    cur.append(p1)
                else:
                    if len(cur) >= 2:
                        chains.append(cur)
                    cur = [p0, p1]
    if len(cur) >= 2:
        chains.append(cur)
    return chains


def main():
    tree = ET.parse('scratch/garmire-to-animate.svg')
    root = tree.getroot()
    ns = {'svg': 'http://www.w3.org/2000/svg'}

    CCX, CCY, RAD = 128.25, 104.33, 104.21
    NORM_SCALE = 0.92 / RAD  # leaves ~8% outer breathing margin inside ±1.0

    def to_norm(x, y):
        return ((x - CCX) * NORM_SCALE, -(y - CCY) * NORM_SCALE)

    # Extract paths from XML cleanly
    all_paths = root.findall('.//svg:path', ns)
    # 12 paths inside clip group (alternating trace, pad, trace, pad, ...), then last path is G!
    assert len(all_paths) == 13, f"Expected 13 paths, found {len(all_paths)}"

    clipped_group = root.find('.//svg:g[@class="cls-2"]', ns)
    clipped_paths = clipped_group.findall('svg:path', ns)
    g_path_el = all_paths[-1]

    # Parse 6 trace+pad pairs
    pads_info = []  # each: {'traces': [...chains...], 'rings': [...chains...], 'center': (x,y)}
    for idx in range(0, len(clipped_paths), 2):
        trace_d = clipped_paths[idx].attrib['d']
        ring_d = clipped_paths[idx + 1].attrib['d']

        trace_chains = []
        for sub in parse_svg_path_subpaths(trace_d, tx=23.92):
            trace_chains.extend(clip_polyline_to_circle(sub, CCX, CCY, RAD))

        ring_chains = []
        for sub in parse_svg_path_subpaths(ring_d, tx=23.92):
            ring_chains.extend(clip_polyline_to_circle(sub, CCX, CCY, RAD))

        # Approximate pad center from first ring points (actually inner ring center)
        all_r_pts = [p for ch in ring_chains for p in ch]
        pcx = sum(p[0] for p in all_r_pts) / len(all_r_pts)
        pcy = sum(p[1] for p in all_r_pts) / len(all_r_pts)
        # Note: exact centers from SVG are (196.08, {26.53,68.21,109.90}) and (60.42, {109.90,151.58,193.26})
        pads_info.append({'traces': trace_chains, 'rings': ring_chains, 'center': (pcx, pcy)})

    exact_pad_centers = [
        (172.16 + 23.92, 26.53),
        (172.16 + 23.92, 68.21),
        (172.16 + 23.92, 109.90),
        (36.50 + 23.92, 109.90),
        (36.50 + 23.92, 151.58),
        (36.50 + 23.92, 193.26),
    ]
    for idx, c in enumerate(exact_pad_centers):
        pads_info[idx]['center'] = c

    g_subs = parse_svg_path_subpaths(g_path_el.attrib['d'], tx=23.92)

    def append_header(bin_buf, frame_num, num_frames, num_rec):
        bin_buf.extend(b'ILDA\x00\x00\x00\x04')  # Format 4 (3D True Color)
        bin_buf.extend(b'GARMIRE ')
        bin_buf.extend(b'GARMIRE ')
        bin_buf.extend(struct.pack('>HHHBB', num_rec, frame_num, num_frames, 0, 0))

    def append_pt(bin_buf, nx, ny, nz, r, g, b, blank, is_last=False):
        ix = max(-32768, min(32767, int(round(nx * 32767.0))))
        iy = max(-32768, min(32767, int(round(ny * 32767.0))))
        iz = max(-32768, min(32767, int(round(nz * 32767.0))))
        st = (0x40 if blank else 0x00) | (0x80 if is_last else 0x00)
        # Format 4 byte order per Spec Fig 6: X, Y, Z, Status, B, G, R
        bin_buf.extend(struct.pack('>hhhBBBB', ix, iy, iz, st, b, g, r))

    NUM_FRAMES = 8
    ilda_bin = bytearray()
    frame_vis = []  # store normalized (pts) for PNG verification

    for f in range(NUM_FRAMES):
        f_pts = []  # (nx, ny, nz, r, g, b, blank)

        def emit_polyline(pts_xy, z_val, color_fn, dwell_blank_hops=True):
            n_pts = [to_norm(px, py) for px, py in pts_xy]
            if len(n_pts) < 2:
                return
            sx, sy = n_pts[0]
            if f_pts and dwell_blank_hops:
                # 1 blank point at previous pos (or start) -> 1 blank arrival at start
                f_pts.append((sx, sy, z_val, 0, 0, 0, True))
            else:
                f_pts.append((sx, sy, z_val, 0, 0, 0, True))

            for k in range(1, len(n_pts)):
                px, py = n_pts[k]
                r, g, b = color_fn(k, len(n_pts) - 1, px, py)
                f_pts.append((px, py, z_val, r, g, b, False))

        # 1. Outer Rainbow Circle (72 steps -> rotating full-spectrum gradient)
        ring_steps = 72
        circle_pts = []
        for s in range(ring_steps + 1):
            ang = 2.0 * math.pi * (s / ring_steps)
            circle_pts.append((CCX + RAD * math.cos(ang), CCY + RAD * math.sin(ang)))

        def circle_color(k, total, nx, ny):
            hue = ((k / total) + (f / NUM_FRAMES)) % 1.0
            rr, gg, bb = colorsys.hsv_to_rgb(hue, 1.0, 1.0)
            return int(rr * 255), int(gg * 255), int(bb * 255)

        emit_polyline(circle_pts, 0.0, circle_color)

        # 2. Circuit Board Traces & Gold Pads (chase + pulse on white flash frame 6)
        # Energized pad schedule across 8 frames: ping-pong chase + all-on flash on frame 6
        active_map = {
            0: {0},          # R1
            1: {1, 3},       # R2 + L1
            2: {2, 4},       # R3 + L2
            3: {5},          # L3
            4: {2, 4},       # R3 + L2
            5: {1, 3},       # R2 + L1
            6: {0, 1, 2, 3, 4, 5},  # All pads surge with G white pulse!
            7: {0, 5},       # Top-right & bottom-left trail
        }[f]

        for p_idx, pdata in enumerate(pads_info):
            energized = (p_idx in active_map)
            # Trace color: emerald green normally -> electric cyan-white when energized
            tr_col = (120, 255, 220) if energized else (0, 175, 80)
            for ch in pdata['traces']:
                emit_polyline(ch, -0.20, lambda k, t, x, y, c=tr_col: c)

            # Pad ring color: warm PCB gold normally -> hot white-gold when energized
            ring_col = (255, 250, 170) if energized else (210, 165, 42)
            for ch in pdata['rings']:
                emit_polyline(ch, -0.10, lambda k, t, x, y, c=ring_col: c)

            # Animated inner spark diamond inside active pad (when clipped inside circle)
            if energized:
                pcx, pcy = pdata['center']
                r_spark = 4.2
                diamond = [
                    (pcx + r_spark, pcy),
                    (pcx, pcy + r_spark),
                    (pcx - r_spark, pcy),
                    (pcx, pcy - r_spark),
                    (pcx + r_spark, pcy),
                ]
                for ch in clip_polyline_to_circle(diamond, CCX, CCY, RAD):
                    emit_polyline(ch, -0.05, lambda k, t, x, y: (255, 255, 255))

        # 3. Stylized G (stays emerald green, pulses to white cleanly at frames 5-7)
        g_colors = {
            0: (0, 235, 96),
            1: (0, 235, 96),
            2: (0, 235, 96),
            3: (0, 235, 96),
            4: (40, 245, 120),
            5: (170, 255, 200),
            6: (255, 255, 255),  # Pure white pulse!
            7: (150, 255, 190),
        }
        gc = g_colors[f]
        for ch in g_subs:
            emit_polyline(ch, 0.25, lambda k, t, x, y, c=gc: c)

        # Write ILDA frame binary
        append_header(ilda_bin, f, NUM_FRAMES, len(f_pts))
        for idx_pt, (nx, ny, nz, r, g, b, blank) in enumerate(f_pts):
            append_pt(ilda_bin, nx, ny, nz, r, g, b, blank, is_last=(idx_pt + 1 == len(f_pts)))

        frame_vis.append(f_pts)
        print(f"Frame {f+1}/{NUM_FRAMES}: {len(f_pts)} ILDA points")

    # EOF header
    append_header(ilda_bin, 0, NUM_FRAMES, 0)

    os.makedirs('scratch', exist_ok=True)
    with open('scratch/garmire-animated.ild', 'wb') as f_out:
        f_out.write(ilda_bin)
    print(f"Wrote scratch/garmire-animated.ild ({len(ilda_bin)} bytes, {NUM_FRAMES} frames)")

    # Write C++ header with embedded 8-frame ILDA binary for Ilda.cpp
    with open('src/garmire_default_ilda.hpp', 'w', encoding='ascii') as fc:
        fc.write('#pragma once\n#include <cstdint>\n\n')
        fc.write('// Auto-generated by scratch/generate_garmire_ilda.py from scratch/garmire-to-animate.svg\n')
        fc.write('// 8-frame ILDA Format 4 sequence: rainbow outer circle, green->white pulsing G, animated test pads\n')
        fc.write('static const uint8_t DEFAULT_GARMIRE_ILDA[] = {\n')
        for i in range(0, len(ilda_bin), 16):
            chunk = ", ".join(f"0x{b:02x}" for b in ilda_bin[i:i+16])
            comma = "," if (i + 16 < len(ilda_bin)) else ""
            fc.write(f"\t{chunk}{comma}\n")
        fc.write('};\n')
    print(f"Wrote src/garmire_default_ilda.hpp ({len(ilda_bin)} bytes)")

    # Render 4x2 frame storyboard PNG to visually inspect all 8 frames
    tile = 260
    sheet = Image.new('RGB', (tile * 4, tile * 2), (16, 16, 20))
    draw = ImageDraw.Draw(sheet)
    for f in range(NUM_FRAMES):
        ox = (f % 4) * tile
        oy = (f // 4) * tile
        cx = ox + tile * 0.5
        cy = oy + tile * 0.5
        rad_px = tile * 0.44
        pts = frame_vis[f]
        draw.rectangle([ox, oy, ox + tile - 1, oy + tile - 1], outline=(40, 40, 50))
        prev = None
        for nx, ny, nz, r, g, b, blank in pts:
            sx = cx + nx * rad_px
            sy = cy - ny * rad_px
            if prev is not None and not blank and (r + g + b) > 0:
                draw.line([prev[0], prev[1], sx, sy], fill=(r, g, b), width=2)
            prev = (sx, sy)
        draw.text((ox + 8, oy + 8), f"F{f+1}/8", fill=(180, 180, 190))
    sheet.save('scratch/garmire_ilda_8frames.png')
    print("Saved 8-frame inspection grid to scratch/garmire_ilda_8frames.png")


if __name__ == '__main__':
    main()
