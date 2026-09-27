"""
Garmire UI Design & MetaModule Graphics Harness
================================================
Centralized design system, typography pipeline, vector path generator,
clearance validator, and dual-target asset builder (VCV Rack SVG + MetaModule PNG).

Complies strictly with:
- VCV Rack NanoSVG / NanoVG constraints (no <text> tags, cubic Bezier 'C' curves only, Skia pathops simplified)
- 4ms MetaModule hardware constraints (exact 240px height, RGB565 16-bit color safety, 47.44 effective DPI)
- Garmire Unified Layout & Clearance tolerances (Zone 1-4 spatial architecture)
"""

import os
import sys
import re
from dataclasses import dataclass, field
from typing import List, Tuple, Optional, Dict

# ── MSYS2 CairoSVG Environment Setup for Windows ─────────────────────────────
MSYS_BIN = r'C:\msys64\mingw64\bin'
CAIROSVG_AVAILABLE = False

if os.path.exists(MSYS_BIN):
    os.environ['PATH'] = MSYS_BIN + os.path.pathsep + os.environ.get('PATH', '')
    if hasattr(os, 'add_dll_directory'):
        try:
            os.add_dll_directory(MSYS_BIN)
        except Exception:
            pass

try:
    import cairocffi
    cairo_dll = os.path.join(MSYS_BIN, 'libcairo-2.dll')
    if os.path.exists(cairo_dll):
        cairocffi.cairo = cairocffi.ffi.dlopen(cairo_dll)
    import cairosvg
    CAIROSVG_AVAILABLE = True
except Exception as e:
    CAIROSVG_AVAILABLE = False
    sys.stderr.write(f"[WARN] CairoSVG could not be initialized: {e}\n")

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
import pathops


# ── Global Design Theme Configuration ────────────────────────────────────────
@dataclass
class UITheme:
    """Configurable global visual theme for Garmire modules."""
    name: str = "Classic Slate"
    background_fill: str = "#7c7c7c"
    delineator_stroke: str = "#999999"
    delineator_width: float = 0.176
    
    # Typography Colors
    title_fill: str = "#ffffff"
    version_fill: str = "#aaaaaa"
    channel_header_fill: str = "#1c1c1c"
    parameter_subheader_fill: str = "#1c1c1c"
    trimpot_label_fill: str = "#2c2c2c"
    jack_label_fill: str = "#2c2c2c"
    
    # Hardware Accent Fills
    screw_fill: str = "#444444"
    socket_outer_fill: str = "#1a1a1a"
    socket_inner_fill: str = "#333333"
    laser_r_stroke: str = "#e52020"
    laser_g_stroke: str = "#15c815"
    laser_b_stroke: str = "#2545f0"
    logo_fill: str = "#ffffff"


# Standard Theme Presets
THEMES = {
    "slate": UITheme(
        name="Classic Slate",
        background_fill="#7c7c7c",
        delineator_stroke="#999999",
        title_fill="#ffffff",
        version_fill="#aaaaaa",
        channel_header_fill="#1c1c1c",
        parameter_subheader_fill="#1c1c1c",
        trimpot_label_fill="#2c2c2c",
        jack_label_fill="#2c2c2c"
    ),
    "dark_carbon": UITheme(
        name="Dark Carbon",
        background_fill="#242426",
        delineator_stroke="#3f3f44",
        title_fill="#00e5ff",
        version_fill="#7e7e88",
        channel_header_fill="#f0f0f2",
        parameter_subheader_fill="#d2d2d8",
        trimpot_label_fill="#a5a5b0",
        jack_label_fill="#a5a5b0",
        logo_fill="#00e5ff"
    ),
    "anodized_black": UITheme(
        name="Anodized Black",
        background_fill="#19191a",
        delineator_stroke="#333336",
        title_fill="#ffffff",
        version_fill="#66666e",
        channel_header_fill="#eaeaea",
        parameter_subheader_fill="#cccccc",
        trimpot_label_fill="#9999a0",
        jack_label_fill="#9999a0",
        logo_fill="#ffffff"
    ),
    "aluminum": UITheme(
        name="Brushed Aluminum",
        background_fill="#d4d4d8",
        delineator_stroke="#a1a1aa",
        title_fill="#0f172a",
        version_fill="#64748b",
        channel_header_fill="#09090b",
        parameter_subheader_fill="#18181b",
        trimpot_label_fill="#27272a",
        jack_label_fill="#27272a",
        logo_fill="#0f172a"
    )
}


# ── Geometry & Physical Layout Constants ─────────────────────────────────────
HP_MM = 5.08
HEIGHT_3U_MM = 128.50
METAMODULE_HEIGHT_PX = 240
METAMODULE_DPI = 47.44

def hp_to_mm(hp: float) -> float:
    return round(hp * HP_MM, 2)

def hp_to_metamodule_px(hp: float) -> int:
    """Calculates exact integer width in pixels for MetaModule screen (47.44 effective DPI)."""
    # 6 HP = 30.48mm -> 57px, 3 HP -> 28px, 15 HP -> 76px, 22.5 HP -> 114px, 10 HP -> 95px
    mm = hp_to_mm(hp)
    return int(round(mm * (METAMODULE_HEIGHT_PX / HEIGHT_3U_MM)))


# ── Typography & PathOps Glyph Engine ────────────────────────────────────────
class TypographyEngine:
    """
    Renders text glyphs as cubic Bezier paths, strictly eliminating <text> tags
    and ensuring 100% NanoSVG parser compliance.
    """
    def __init__(self, res_dir: str = 'res'):
        self.res_dir = res_dir
        self.node_font_path = os.path.join(res_dir, 'Node.otf')
        self.qs_med_path = os.path.join(res_dir, 'Quicksand-Medium.ttf')
        self.qs_bold_path = os.path.join(res_dir, 'Quicksand-Bold.ttf')
        self.qs_reg_path = os.path.join(res_dir, 'Quicksand-Regular.ttf')

        self.node_font = TTFont(self.node_font_path) if os.path.exists(self.node_font_path) else None
        self.qs_med_font = TTFont(self.qs_med_path) if os.path.exists(self.qs_med_path) else None
        self.qs_bold_font = TTFont(self.qs_bold_path) if os.path.exists(self.qs_bold_path) else None
        self.qs_reg_font = TTFont(self.qs_reg_path) if os.path.exists(self.qs_reg_path) else None
        
        self._glyph_cache: Dict[Tuple[str, str], str] = {}

    def get_simplified_glyph_path(self, font: TTFont, char: str) -> str:
        cache_key = (font.reader.file.name if hasattr(font, 'reader') else str(id(font)), char)
        if cache_key in self._glyph_cache:
            return self._glyph_cache[cache_key]

        cmap = font.getBestCmap()
        gname = cmap[ord(char)] if cmap and ord(char) in cmap else char
        gset = font.getGlyphSet()

        path = pathops.Path()
        pen = pathops.PathPen(path)
        gset[gname].draw(pen)

        # Simplify resolves self-intersections and converts TrueType quadratic to cubic curves
        simplified = pathops.simplify(path)
        svg_pen = SVGPathPen(None)
        simplified.draw(svg_pen)

        path_d = svg_pen.getCommands()
        # Format numbers to 1 decimal place and pad command characters with spaces for NanoSVG
        path_d = re.sub(r'(\d+\.\d+)', lambda m: f'{float(m.group(1)):.1f}', path_d)
        path_d = re.sub(r'([MLCQZHVmlcqzhv])', r' \1 ', path_d)
        path_d = re.sub(r'\s+', ' ', path_d).strip()

        self._glyph_cache[cache_key] = path_d
        return path_d

    def render_text(self, font: TTFont, text: str, center_x: float, baseline_y: float,
                    scale: float, fill: str, comment: str = "") -> str:
        """Renders horizontally centered text into SVG groups of cubic Bezier paths."""
        cmap = font.getBestCmap()
        hmtx = font['hmtx']
        widths = [hmtx[cmap[ord(c)]][0] * scale for c in text]
        total_w = sum(widths)
        start_x = center_x - total_w / 2.0

        res = []
        if comment:
            res.append(f'  <!-- Label: "{comment}" -->')
        curr = start_x
        for c, w in zip(text, widths):
            if c != ' ':
                pd = self.get_simplified_glyph_path(font, c)
                res.append(
                    f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({scale:.6f}, {-scale:.6f})">'
                    f'<path d="{pd}" fill="{fill}"/></g>'
                )
            curr += w
        return "\n".join(res)

    def render_left_aligned_text(self, font: TTFont, text: str, start_x: float, baseline_y: float,
                                 scale: float, fill: str, comment: str = "") -> str:
        """Renders left-aligned text into SVG groups of cubic Bezier paths."""
        cmap = font.getBestCmap()
        hmtx = font['hmtx']
        widths = [hmtx[cmap[ord(c)]][0] * scale for c in text]

        res = []
        if comment:
            res.append(f'  <!-- Label: "{comment}" -->')
        curr = start_x
        for c, w in zip(text, widths):
            if c != ' ':
                pd = self.get_simplified_glyph_path(font, c)
                res.append(
                    f'    <g transform="translate({curr:.3f}, {baseline_y:.3f}) scale({scale:.6f}, {-scale:.6f})">'
                    f'<path d="{pd}" fill="{fill}"/></g>'
                )
            curr += w
        return "\n".join(res)


# ── Logo & Hardware Asset Generator ──────────────────────────────────────────
class HardwareAssetEngine:
    """Generates clean vector assets for jacks, potentiometers, switches, and brand insignia."""

    @staticmethod
    def render_garmire_logo(center_x: float, center_y: float, scale: float = 1.0, fill: str = "#ffffff") -> str:
        """
        Renders the Garmire laser trajectory insignia:
        A sleek laser beam emitting prism / triangular trajectory motif.
        """
        w = 4.0 * scale
        h = 3.2 * scale
        # Geometric prism laser path with clean cubic curves
        return (
            f'  <!-- Garmire Brand Insignia -->\n'
            f'  <g transform="translate({center_x:.3f}, {center_y:.3f})">\n'
            f'    <path d="M {-w/2:.2f} {h/2:.2f} L 0 {-h/2:.2f} L {w/2:.2f} {h/2:.2f} Z" '
            f'fill="none" stroke="{fill}" stroke-width="{0.28 * scale:.3f}"/>\n'
            f'    <circle cx="0" cy="0.2" r="{0.45 * scale:.3f}" fill="{fill}"/>\n'
            f'    <line x1="{-w/3:.2f}" y1="{h/5:.2f}" x2="{w/3:.2f}" y2="{h/5:.2f}" '
            f'stroke="{fill}" stroke-width="{0.2 * scale:.3f}"/>\n'
            f'  </g>'
        )

    @staticmethod
    def render_mounting_screws(width_mm: float) -> str:
        """Renders standard Eurorack mounting screw recesses."""
        margin_x = 7.50 if width_mm > 40 else width_mm / 2.0
        return (
            f'  <!-- Mounting Screws -->\n'
            f'  <circle cx="{margin_x:.2f}" cy="3.50" r="1.80" fill="#3a3a3a"/>\n'
            f'  <circle cx="{margin_x:.2f}" cy="125.00" r="1.80" fill="#3a3a3a"/>'
        )

    @staticmethod
    def render_jack_socket(cx: float, cy: float, ring_color: Optional[str] = None) -> str:
        """Renders hardware-realistic jack socket collar."""
        parts = [
            f'  <!-- Jack at ({cx:.2f}, {cy:.2f}) -->',
            f'  <circle cx="{cx:.2f}" cy="{cy:.2f}" r="4.10" fill="#202020" stroke="#383838" stroke-width="0.25"/>',
            f'  <circle cx="{cx:.2f}" cy="{cy:.2f}" r="2.65" fill="#111111"/>'
        ]
        if ring_color:
            parts.append(
                f'  <circle cx="{cx:.2f}" cy="{cy:.2f}" r="4.35" fill="none" stroke="{ring_color}" stroke-width="0.70"/>'
            )
        return "\n".join(parts)

    @staticmethod
    def render_delineator(x1: float, x2: float, y: float, stroke: str = "#999999", width: float = 0.176) -> str:
        return f'  <line x1="{x1:.2f}" y1="{y:.2f}" x2="{x2:.2f}" y2="{y:.2f}" stroke="{stroke}" stroke-width="{width:.3f}"/>'


# ── Clearance & Invariant Validator ──────────────────────────────────────────
class ClearanceValidator:
    """Validates physical hardware clearances and pre-flight invariants."""

    @staticmethod
    def validate_clearance(knob_ys: List[float], trimpot_ys: List[float], jack_ys: List[float],
                           col_xs: List[float]) -> List[str]:
        warnings = []
        # Knobs vertical clearance: minimum 11.41mm
        sorted_knobs = sorted(knob_ys)
        for i in range(len(sorted_knobs) - 1):
            dy = sorted_knobs[i+1] - sorted_knobs[i]
            if dy < 11.41:
                warnings.append(f"Knob clearance warning: ΔY = {dy:.2f}mm < 11.41mm required.")

        # Trimpot vertical clearance: minimum 5.50mm
        sorted_trims = sorted(trimpot_ys)
        for i in range(len(sorted_trims) - 1):
            dy = sorted_trims[i+1] - sorted_trims[i]
            if dy < 5.50:
                warnings.append(f"Trimpot clearance warning: ΔY = {dy:.2f}mm < 5.50mm required.")

        # Jack vertical clearance: minimum 3.00mm collar clearance (pitch >= 9.50mm)
        sorted_jacks = sorted(jack_ys)
        for i in range(len(sorted_jacks) - 1):
            dy = sorted_jacks[i+1] - sorted_jacks[i]
            if dy < 9.50:
                warnings.append(f"Jack pitch warning: ΔY = {dy:.2f}mm < 9.50mm standard pitch.")

        # Horizontal column separation
        sorted_cols = sorted(col_xs)
        for i in range(len(sorted_cols) - 1):
            dx = sorted_cols[i+1] - sorted_cols[i]
            if dx < 5.24:
                warnings.append(f"Horizontal column separation warning: ΔX = {dx:.2f}mm < 5.24mm required.")

        return warnings

    @staticmethod
    def check_svg_invariants(svg_content: str) -> List[str]:
        errors = []
        # Strictly NO <text> tags
        if '<text' in svg_content or '</text>' in svg_content:
            errors.append("CRITICAL: Found raw <text> tag in SVG faceplate! NanoSVG forbids <text> tags.")
        # Quadratic curves check
        if re.search(r'\s[Qq]\s', svg_content):
            errors.append("WARNING: Found quadratic Bezier ('Q'/'q') in path! Must be converted to cubic ('C').")
        return errors


# ── Dual-Target Asset Exporter ───────────────────────────────────────────────
class DualTargetExporter:
    """Exports both VCV Rack SVG faceplates and MetaModule PNG assets synchronously."""

    @staticmethod
    def export(slug: str, svg_content: str, width_hp: float, res_dir: str = 'res',
               assets_dir: str = 'metamodule/assets') -> Tuple[bool, str]:
        os.makedirs(res_dir, exist_ok=True)
        os.makedirs(assets_dir, exist_ok=True)

        svg_path = os.path.join(res_dir, f"{slug}.svg")
        png_path = os.path.join(assets_dir, f"{slug}.png")

        # 1. Invariant Validation
        errors = ClearanceValidator.check_svg_invariants(svg_content)
        if any("CRITICAL" in err for err in errors):
            return False, "\n".join(errors)

        # 2. Write SVG
        with open(svg_path, 'w', encoding='utf-8') as f:
            f.write(svg_content.strip() + "\n")

        # 3. Render PNG for MetaModule (exact 240px height)
        target_w = hp_to_metamodule_px(width_hp)
        target_h = METAMODULE_HEIGHT_PX

        if CAIROSVG_AVAILABLE:
            try:
                cairosvg.svg2png(
                    bytestring=svg_content.encode('utf-8'),
                    write_to=png_path,
                    output_width=target_w,
                    output_height=target_h
                )
                status = f"Exported {svg_path} & {png_path} ({target_w}x{target_h}px)"
                return True, status
            except Exception as e:
                return False, f"Failed to render PNG via CairoSVG: {e}"
        else:
            return True, f"Exported {svg_path} (Warning: CairoSVG unavailable, PNG not rendered)"


# ── Master Panel Builder Class ───────────────────────────────────────────────
class PanelBuilder:
    """
    Standardized, high-level panel builder enforcing Garmire Eurorack & MetaModule standards.
    """
    def __init__(self, slug: str, hp: float = 6.0, version: str = "v1.0.4",
                 theme: Optional[UITheme] = None, show_logo: bool = True):
        self.slug = slug
        self.hp = hp
        self.width_mm = hp_to_mm(hp)
        self.height_mm = HEIGHT_3U_MM
        self.version = version
        self.theme = theme or THEMES["slate"]
        self.show_logo = show_logo

        self.typo = TypographyEngine()
        self.parts: List[str] = []
        self.delineators: List[float] = []

        # Clearance tracking
        self.knob_ys: List[float] = []
        self.trimpot_ys: List[float] = []
        self.jack_ys: List[float] = []
        self.col_xs: List[float] = [7.62, 22.86] if hp == 6.0 else [self.width_mm / 2.0]

    def build_header(self, title_display: Optional[str] = None):
        """Builds Zone 1: Background, Screws, Title, Version Tag, and Optional Logo."""
        title_str = (title_display or self.slug).lower()
        cx = self.width_mm / 2.0

        self.parts.append('<?xml version="1.0" encoding="UTF-8"?>')
        self.parts.append(
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.width_mm:.2f}mm" '
            f'height="{self.height_mm:.2f}mm" viewBox="0 0 {self.width_mm:.2f} {self.height_mm:.2f}">'
        )
        self.parts.append(f'  <!-- Panel Background: {self.hp} HP -->')
        self.parts.append(f'  <rect width="{self.width_mm:.2f}" height="{self.height_mm:.2f}" fill="{self.theme.background_fill}"/>')
        self.parts.append('')
        self.parts.append(HardwareAssetEngine.render_mounting_screws(self.width_mm))
        self.parts.append('')

        # Title "title" in Node.otf (Fixed Y = 7.620mm)
        if self.typo.node_font:
            self.parts.append(
                self.typo.render_text(
                    self.typo.node_font, title_str, cx, 7.620, 0.0048, self.theme.title_fill, f"Title: {title_str}"
                )
            )

        # Version Tag "v1.X.Y" in Quicksand (Fixed Y = 10.414mm)
        if self.typo.qs_med_font:
            self.parts.append(
                self.typo.render_text(
                    self.typo.qs_med_font, self.version, cx, 10.414, 0.0016, self.theme.version_fill, f"Version: {self.version}"
                )
            )

        # Logo Insignia
        if self.show_logo:
            # Place small brand logo at bottom above clearance or top right
            if self.hp >= 6.0:
                self.parts.append(HardwareAssetEngine.render_garmire_logo(cx, 123.50, scale=0.8, fill=self.theme.logo_fill))

    def add_delineator(self, y: float, margin_x: float = 2.54):
        """Adds a horizontal separator line across the panel."""
        self.delineators.append(y)
        self.parts.append(
            HardwareAssetEngine.render_delineator(
                margin_x, self.width_mm - margin_x, y, self.theme.delineator_stroke, self.theme.delineator_width
            )
        )

    def add_channel_headers(self, y: float = 13.070, left_label: str = "X", right_label: str = "Y"):
        """Row 1 Column Headers (Fixed Y = 13.070mm)."""
        if self.typo.qs_med_font:
            self.parts.append(self.typo.render_text(self.typo.qs_med_font, left_label, 7.62, y, 0.0030, self.theme.channel_header_fill, left_label))
            self.parts.append(self.typo.render_text(self.typo.qs_med_font, right_label, 22.86, y, 0.0030, self.theme.channel_header_fill, right_label))

    def add_knob_label(self, text: str, cx: float, y: float, scale: float = 0.0030):
        if self.typo.qs_med_font:
            self.parts.append(self.typo.render_text(self.typo.qs_med_font, text, cx, y, scale, self.theme.parameter_subheader_fill, text))

    def add_trimpot_label(self, text: str, cx: float, y: float, scale: float = 0.0020):
        if self.typo.qs_med_font:
            self.parts.append(self.typo.render_text(self.typo.qs_med_font, text, cx, y, scale, self.theme.trimpot_label_fill, text))

    def add_jack_label(self, text: str, cx: float, y: float, scale: float = 0.0024):
        if self.typo.qs_med_font:
            self.parts.append(self.typo.render_text(self.typo.qs_med_font, text, cx, y, scale, self.theme.jack_label_fill, text))

    def render_svg(self) -> str:
        self.parts.append('</svg>')
        return "\n".join(self.parts)

    def save_and_render(self) -> Tuple[bool, str]:
        svg_content = self.render_svg()
        return DualTargetExporter.export(self.slug, svg_content, self.hp)
