#!/usr/bin/env python3
import os
import sys
import json
import xml.etree.ElementTree as ET
from http.server import HTTPServer, BaseHTTPRequestHandler
import urllib.parse

REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PALETTE_DIR = os.path.join(REPO_DIR, "scratch", "palette")
PALETTE_FILE = os.path.join(PALETTE_DIR, "palette.json")
PLUGIN_JSON = os.path.join(REPO_DIR, "plugin.json")
RES_DIR = os.path.join(REPO_DIR, "res")

def get_module_data():
    if not os.path.exists(PLUGIN_JSON):
        return []
    with open(PLUGIN_JSON, "r", encoding="utf-8") as f:
        pdata = json.load(f)
    
    saved_palettes = {}
    if os.path.exists(PALETTE_FILE):
        try:
            with open(PALETTE_FILE, "r", encoding="utf-8") as f:
                saved_palettes = json.load(f)
        except Exception:
            saved_palettes = {}

    modules = []
    for m in pdata.get("modules", []):
        slug = m.get("slug")
        name = m.get("name", slug)
        svg_file = os.path.join(RES_DIR, f"{slug}.svg")
        
        hp = 6  # default
        svg_content = ""
        if os.path.exists(svg_file):
            try:
                with open(svg_file, "r", encoding="utf-8") as sf:
                    svg_content = sf.read()
                # Parse width from SVG
                tree = ET.fromstring(svg_content)
                w_str = tree.get("width", "")
                if "mm" in w_str:
                    w_mm = float(w_str.replace("mm", "").strip())
                    hp = round(w_mm / 5.08)
                else:
                    vb = tree.get("viewBox", "")
                    parts = vb.split()
                    if len(parts) == 4:
                        w_vb = float(parts[2])
                        hp = round(w_vb / 5.08)
            except Exception as e:
                print(f"Error parsing SVG for {slug}: {e}")

        has_palette = slug in saved_palettes
        modules.append({
            "slug": slug,
            "name": name,
            "hp": hp,
            "hasPalette": has_palette,
            "palette": saved_palettes.get(slug, None),
            "svg": svg_content
        })
    return modules

def generate_modules_js():
    modules = get_module_data()
    js_path = os.path.join(PALETTE_DIR, "modules_data.js")
    with open(js_path, "w", encoding="utf-8") as f:
        f.write("// Auto-generated module catalog from plugin.json and res/*.svg\n")
        f.write("window.MODULES_CATALOG = ")
        json.dump(modules, f, indent=2)
        f.write(";\n")
    print(f"Updated {js_path} with {len(modules)} modules.")

class PaletteHandler(BaseHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path in ["", "/", "/index.html"]:
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            with open(os.path.join(PALETTE_DIR, "index.html"), "rb") as f:
                self.wfile.write(f.read())
            return

        elif path == "/modules_data.js":
            self.send_response(200)
            self.send_header("Content-Type", "application/javascript; charset=utf-8")
            self.end_headers()
            with open(os.path.join(PALETTE_DIR, "modules_data.js"), "rb") as f:
                self.wfile.write(f.read())
            return

        elif path == "/palette.json" or path == "/api/palette":
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.end_headers()
            if os.path.exists(PALETTE_FILE):
                with open(PALETTE_FILE, "rb") as f:
                    self.wfile.write(f.read())
            else:
                self.wfile.write(b"{}")
            return

        elif path == "/api/modules":
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.end_headers()
            mods = get_module_data()
            self.wfile.write(json.dumps(mods).encode("utf-8"))
            return

        elif path.startswith("/res/"):
            fname = path[5:]
            fpath = os.path.join(RES_DIR, fname)
            if os.path.exists(fpath):
                self.send_response(200)
                if fname.endswith(".svg"):
                    self.send_header("Content-Type", "image/svg+xml")
                elif fname.endswith(".ttf") or fname.endswith(".otf"):
                    self.send_header("Content-Type", "font/opentype")
                else:
                    self.send_header("Content-Type", "application/octet-stream")
                self.end_headers()
                with open(fpath, "rb") as f:
                    self.wfile.write(f.read())
                return
            else:
                self.send_response(404)
                self.end_headers()
                return

        # Fallback to local files
        loc = os.path.join(PALETTE_DIR, path.lstrip("/"))
        if os.path.exists(loc) and os.path.isfile(loc):
            self.send_response(200)
            self.end_headers()
            with open(loc, "rb") as f:
                self.wfile.write(f.read())
            return

        self.send_response(404)
        self.end_headers()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path in ["/api/save", "/api/palette"]:
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length)
            try:
                payload = json.loads(body.decode("utf-8"))
                slug = payload.get("slug")
                data = payload.get("palette", payload)

                saved_palettes = {}
                if os.path.exists(PALETTE_FILE):
                    try:
                        with open(PALETTE_FILE, "r", encoding="utf-8") as f:
                            saved_palettes = json.load(f)
                    except Exception:
                        saved_palettes = {}

                saved_palettes[slug] = data

                with open(PALETTE_FILE, "w", encoding="utf-8") as f:
                    json.dump(saved_palettes, f, indent=2)

                # Re-generate modules_data.js to keep static cache in sync
                generate_modules_js()

                self.send_response(200)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": True, "slug": slug}).encode("utf-8"))
                return
            except Exception as e:
                self.send_response(500)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": False, "error": str(e)}).encode("utf-8"))
                return

        self.send_response(404)
        self.end_headers()

def run_server(port=5050):
    generate_modules_js()
    server_address = ("", port)
    httpd = HTTPServer(server_address, PaletteHandler)
    print(f"\n=======================================================")
    print(f"  FAC 73 Module Palette Server active at:")
    print(f"  http://localhost:{port}/")
    print(f"=======================================================\n")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server.")
        httpd.server_close()

if __name__ == "__main__":
    port = 5050
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    run_server(port)
