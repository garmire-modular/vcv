---
title: 'Agent Directives: VCV Rack & 4ms MetaModule Development'
created: '2026-09-26T19:47:48.812Z'
modified: '2026-11-03T08:16:00.000Z'
---

# Agent Directives: VCV Rack & 4ms MetaModule Development

You are an expert audio DSP engineer and systems programmer working in this repository. All code, vector artwork, and configuration files must comply strictly with VCV Rack v2 and 4ms MetaModule hardware constraints.

> [!IMPORTANT]
> **Workspace Path Directive**: All agents MUST target `G:\dupe\work\garmire` for all file paths, tool arguments, scripts, and terminal commands.

---

## 1. Mandatory Pre-Flight Checklist

Before writing or modifying any code, SVGs, or build configurations, you MUST output a planning block containing this verified checklist:

```markdown
### Pre-Flight Invariant Verification
- [ ] **Architecture Check:** DSP changes preserve real-time safety (no malloc, free, locks, or file I/O in `process()`).
- [ ] **Task Scope & Directives:** Consulted relevant specialized guide (`docs/module_design.md` for UI/panels, or `docs/classes/<class>.md` for module functional class standards).
- [ ] **Version Tracking:** Per-module version tracked independently (starts at `v1.0.0`, autoincremented on changes) and synchronized across files.
- [ ] **Automated Compliance:** Verified with `python scratch/verify_compliance.py` prior to commits.
- [ ] **Build & Deploy:** Concurrency check executed before build; changes deployed via `make install`.
```

---

## 2. Git Version Control & Remote Sync

- **Local Commits on Every Completed Task/Change**:
  - Whenever code, panel scripts, or asset changes are verified (compilation succeeds and pre-flight invariants are satisfied), stage the relevant source files and create a clean, descriptive local commit (e.g. `feat(...)`, `fix(...)`, `panel(...)`, `docs(...)`).
  - Never commit build artifacts (`*.dll`, `*.vcvplugin`, `build/`, `dist/`).
- **Per-Module Versioning & Autoincrementation Protocol**:
  - **Individual Module Version**: Each module tracks its own independent semantic version starting at **`v1.0.0`** upon initial creation.
  - **Module Version Autoincrementation**: Every time a module undergoes changes (DSP fixes, UI adjustments, feature updates), its individual version MUST be bumped (e.g. `v1.0.0` $\rightarrow$ `v1.0.1` for fixes/refinements, `v1.1.0` for feature additions).
  - **Panel & Asset Synchrony**: The per-module version is stamped on the faceplate SVG (`scratch/generate_<module>_panel.py`), rendered to `res/<Module>.svg`, and mirrored in `metamodule/assets/<Module>.png`.
  - **Plugin Suite Version Separation**: The overall plugin suite version in `plugin.json` (`"version": "2.26.0"`) is managed separately at the release/package level. Never jump an initial or individual module version to match the suite's global `2.26.0`.
- **Prompt to Push to GitHub on Milestones**:
  - After creating a new module or completing a substantial revision, explicitly ask the user:
    > *"Would you like to push to GitHub?"*
  - Do NOT push to GitHub automatically without user confirmation. Once confirmed by the user, push to the designated GitHub remote (`git push origin <branch>`).

---

## 3. Build & Compilation Concurrency Control

To prevent race conditions, file locks on `plugin.dll` or `build/` objects, and compiler conflicts:
- **Pre-Compilation Concurrency Check**:
  - Before running any `make` or build command, inspect running processes:
    ```powershell
    Get-Process make, g++, gcc, ninja -ErrorAction SilentlyContinue
    ```
- **Concurrency Wait Protocol**:
  - If any compilation process is currently running:
    - Do **NOT** start a new compilation.
    - Wait **25 seconds** before checking process status again.
    - Repeat the 25-second wait loop until all conflicting compilation processes have exited.
  - Proceed with the `make` invocation **only** when no other compilation is in progress.

---

## 4. Mandatory Pre-Commit Compliance Verification & Plugin Deployment (`make install`)

### 4.1. Automated Compliance Verification
Before committing code, vector panels, or metadata changes, you MUST run the automated compliance linter:
```powershell
python scratch/verify_compliance.py
```
This script automatically enforces:
- **Virtual Screws**: Zero `ScrewSilver` or `ScrewBlack` instances in `ModuleWidget` or panel SVGs.
- **Port Tooltips**: No redundant `"input"`, `"output"`, `"IN"`, or `"OUT"` words in `configInput()` or `configOutput()`.
- **Attenuverter Tooltips**: Sentence-case format strictly following `<Parameter> CV depth` (or `X <Parameter> CV depth`).
- **Sentence Case**: All `configParam()`, `configInput()`, and `configOutput()` tooltips use sentence case.
- **SVG Integrity**: Zero `<text>` tags (all fonts must be cubic Bezier paths) and zero `<style>` blocks (Illustrator export fix).
- **Taxonomy**: Valid functional class assigned in `plugin.json`.

If any `[ERROR]` is flagged, it must be resolved prior to committing or installing.

### 4.2. Plugin Directory Deployment (`make install`)
Whenever compiling or verifying changes to the VCV Rack plugin:
- Always copy the compiled Rack files to active user plugin directories for immediate testing.
- Run `make install` (or deploy `plugin.dll`, `plugin.json`, `res/`, and `dist/*.vcvplugin` to `C:/Users/Pat/AppData/Local/Rack2/plugins-win-x64/Garmire` and `C:/Users/Pat/AppData/Local/Rack2/plugins-win-x64`).

---

## 5. Task Routing & Modular Directives

To keep instructions focused and avoid token overhead, detailed task specifications are separated into specialized reference documents. **Load and follow the appropriate document based on the current task:**

### 5.1. Module Panel Design & UI Layout
When tasked with designing, sizing, or modifying module panels, widget layout, custom hardware widgets, font rendering, palette badge, or theme skins:
- Read and follow [docs/module_design.md](file:///g:/dupe/work/garmire/docs/module_design.md).
- **Panel Edit Invariant**: Vector modifications MUST target `scratch/generate_<module>_panel.py`, NEVER direct edits to `res/<Module>.svg`.
- For font metrics and scale factors, see [docs/TYPOGRAPHY.md](file:///g:/dupe/work/garmire/docs/TYPOGRAPHY.md).

### 5.2. Functional Taxonomy & Module Classes
Every Garmire module belongs to one of four functional classes. When scaffolding, designing, or implementing a module, **explicitly prompt the user** to confirm/assign which of the 4 classes it belongs to:
> *"Which functional class does this module belong to: Generate, Transform, Utility, or Control?"*

Tag the confirmed class in `plugin.json` and consult the class-specific directive:

| Class | Scope | Specialized Directives File |
| :--- | :--- | :--- |
| **Generate** | Oscillators, chaos engines, sources, pattern generators | [docs/classes/generate.md](file:///g:/dupe/work/garmire/docs/classes/generate.md) *(Mandates 6 output voltage standards, right-click menu, JSON persistence)* |
| **Transform / Effect** | Waveshapers, filters, slew, warpers, math, effects | [docs/classes/transform.md](file:///g:/dupe/work/garmire/docs/classes/transform.md) or [docs/classes/effect.md](file:///g:/dupe/work/garmire/docs/classes/effect.md) |
| **Utility** | Mixers, attenuverters, switches, mutes, routing, splitters | [docs/classes/utility.md](file:///g:/dupe/work/garmire/docs/classes/utility.md) |
| **Control** | Envelopes, CV sources, sequencers, macro controllers, LFOs | [docs/classes/control.md](file:///g:/dupe/work/garmire/docs/classes/control.md) |
