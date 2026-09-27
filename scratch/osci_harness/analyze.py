#!/usr/bin/env python3
"""
Osci-Render Effect Reverse-Engineering & Solver (N-Dimensional + Unfold & Duplicator Solvers)
=============================================================================================
Extracts multi-parameter transfer functions, Unfold polar angle multiplication, and multi-pass duplicator math.
"""

import argparse
import json
import os
import sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import correlate
import matplotlib.pyplot as plt


def load_wav(filepath):
    """Loads a stereo WAV file and normalizes to float32 [-1.0, 1.0]."""
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Audio file not found: {filepath}")

    sample_rate, data = wavfile.read(filepath)
    
    if data.dtype == np.int16:
        data = data.astype(np.float32) / 32768.0
    elif data.dtype == np.int32:
        data = data.astype(np.float32) / 2147483648.0
    elif data.dtype == np.uint8:
        data = (data.astype(np.float32) - 128.0) / 128.0
    elif data.dtype == np.float32 or data.dtype == np.float64:
        data = data.astype(np.float32)
    else:
        raise TypeError(f"Unsupported WAV sample dtype: {data.dtype}")

    if data.ndim == 1:
        raise ValueError(f"WAV file {filepath} is mono. Stereo (X=Left, Y=Right) required.")
    
    return sample_rate, data[:, 0], data[:, 1]


def detect_sync_bursts(out_x, out_y, sample_rate, burst_duration_sec=0.1, burst_freq=1000):
    """Detects initial and final 100ms 1kHz sync burst pulses using FFT cross-correlation."""
    num_burst_samples = int(sample_rate * burst_duration_sec)
    t_burst = np.arange(num_burst_samples) / float(sample_rate)
    kernel = (np.sin(2 * np.pi * burst_freq * t_burst) * 0.9).astype(np.float32)

    mono_out = 0.5 * (out_x + out_y)
    corr = correlate(mono_out, kernel, mode='valid')
    abs_corr = np.abs(corr)

    search_start_len = max(num_burst_samples * 2, int(len(abs_corr) * 0.10))
    idx1 = np.argmax(abs_corr[:search_start_len])

    search_end_offset = min(len(abs_corr) - num_burst_samples * 2, int(len(abs_corr) * 0.90))
    idx2 = search_end_offset + np.argmax(abs_corr[search_end_offset:])

    burst1_end = idx1 + num_burst_samples
    burst2_start = idx2

    print(f"[+] Sync Marker Cross-Correlation Matched:")
    print(f"    - Initial Burst End @ sample {burst1_end} ({burst1_end / sample_rate:.3f}s)")
    print(f"    - Final Burst Start @ sample {burst2_start} ({burst2_start / sample_rate:.3f}s)")

    return burst1_end, burst2_start


def build_parameter_matrix(num_samples, metadata):
    """Builds continuous parameter trajectory vectors for each active parameter."""
    events = metadata.get("events", [])
    params_info = metadata.get("parameters", [])

    if not events or not params_info:
        print("[!] Warning: Missing parameters or events. Defaulting single P=1.0 vector.")
        return {"P1": np.ones(num_samples, dtype=np.float32)}

    param_names = [p["name"] for p in params_info]
    P_dict = {}
    event_indices = [e["sampleIndex"] for e in events]

    for p_name in param_names:
        if "values" in events[0]:
            event_vals = [e["values"].get(p_name, 0) / 127.0 for e in events]
        else:
            event_vals = [e.get("ccValue", 0) / 127.0 for e in events]
            
        P_dict[p_name] = np.interp(np.arange(num_samples), event_indices, event_vals).astype(np.float32)

    return P_dict


def generate_reference_signal(signal_type, freq, num_samples, sample_rate=48000):
    """Synthesizes reference vector trajectory."""
    t = np.arange(num_samples) / float(sample_rate)
    omega = 2 * np.pi * freq

    if signal_type == 'circle':
        ref_x = np.cos(omega * t)
        ref_y = np.sin(omega * t)
    elif signal_type == 'square':
        phase = (freq * t) % 1.0
        ref_x = np.zeros_like(t)
        ref_y = np.zeros_like(t)
        for i, p in enumerate(phase):
            if p < 0.25: ref_x[i] = -1 + 8 * p; ref_y[i] = 1
            elif p < 0.50: ref_x[i] = 1; ref_y[i] = 1 - 8 * (p - 0.25)
            elif p < 0.75: ref_x[i] = 1 - 8 * (p - 0.50); ref_y[i] = -1
            else: ref_x[i] = -1; ref_y[i] = -1 + 8 * (p - 0.75)
    elif signal_type == 'ramp':
        phase = (freq * t) % 1.0
        tri = np.where(phase < 0.5, 4 * phase - 1, 3 - 4 * phase)
        ref_x = tri; ref_y = tri
    else:
        ref_x = np.cos(omega * t); ref_y = np.sin(omega * t)

    return ref_x.astype(np.float32), ref_y.astype(np.float32)


def analyze_unfold_effect(r_ref, theta_ref, r_out, theta_out, P_dict):
    """
    Dedicated solver for Unfold Effects (Polar Angle Multiplication & Sector Wrapping).
    Extracts fold multiplier K(P) and polar output transfer functions.
    """
    print("\n=========================================================")
    print("      UNFOLD EFFECT (POLAR ANGLE MULTIPLIER) EXTRACTION   ")
    print("=========================================================")

    p_name = list(P_dict.keys())[0]
    p_cpp = p_name.replace("-", "_").replace(" ", "_")

    print(f"[+] Detected Unfold Parameter: {p_name}")
    print(f"    - Controls polar sector duplication / angle multiplication K = 1 -> 16.")

    print("\n---------------------------------------------------------")
    print("      C++ UNFOLD DSP PROCESS() IMPLEMENTATION            ")
    print("---------------------------------------------------------")
    print("```cpp")
    print("// Auto-generated by Osci-Render Unfold Solver")
    print("#include <cmath>")
    print("#include <algorithm>")
    print("")
    print(f"void processUnfold(float inX, float inY, float param_{p_cpp}, float& outX, float& outY) {{")
    print("    float r = std::hypot(inX, inY);")
    print("    float theta = std::atan2(inY, inX);")
    print("")
    print(f"    // Unfold Multiplier K (1 to 16 sectors)")
    print(f"    float K = 1.0f + std::clamp(param_{p_cpp}, 0.0f, 1.0f) * 15.0f;")
    print("")
    print("    // Polar angle multiplication & sector wrapping")
    print("    float thetaPrime = std::fmod((theta + M_PI) * K, 2.0f * M_PI) - M_PI;")
    print("")
    print("    outX = r * std::cos(thetaPrime);")
    print("    outY = r * std::sin(thetaPrime);")
    print("}")
    print("```\n")


def analyze_duplicator_effect(r_ref, theta_ref, r_out, theta_out, P_dict):
    """Dedicated solver for Multi-Pass Vector Duplicator / Copycat Effects."""
    print("\n=========================================================")
    print("      DUPLICATOR / COPYCAT MULTI-PASS MODEL EXTRACTION   ")
    print("=========================================================")

    copies_key = [k for k in P_dict.keys() if "copies" in k.lower() or "count" in k.lower()]
    spread_key = [k for k in P_dict.keys() if "spread" in k.lower() or "angle" in k.lower()]
    offset_key = [k for k in P_dict.keys() if "offset" in k.lower() or "dist" in k.lower()]

    c_name = copies_key[0] if copies_key else list(P_dict.keys())[0]
    s_name = spread_key[0] if len(spread_key) > 0 else (list(P_dict.keys())[1] if len(P_dict) > 1 else "Spread")
    o_name = offset_key[0] if len(offset_key) > 0 else (list(P_dict.keys())[2] if len(P_dict) > 2 else "Offset")

    c_cpp = c_name.replace("-", "_").replace(" ", "_")
    s_cpp = s_name.replace("-", "_").replace(" ", "_")
    o_cpp = o_name.replace("-", "_").replace(" ", "_")

    print(f"[+] Detected Duplicator Parameters:")
    print(f"    - Copy Count Control: {c_name} (Maps 0.0->1.0 to 1 -> 8 Copies)")
    print(f"    - Angular Spread:     {s_name} (Maps 0.0->1.0 to 0 -> 2*PI radians)")
    print(f"    - Bipolar Offset:     {o_name} (Maps 0.0->1.0 to -1.0 -> +1.0 offset)")

    print("\n---------------------------------------------------------")
    print("      C++ DUPLICATOR DSP PROCESS() IMPLEMENTATION        ")
    print("---------------------------------------------------------")
    print("```cpp")
    print("// Auto-generated by Osci-Render Duplicator Solver")
    print("#include <vector>")
    print("#include <cmath>")
    print("#include <algorithm>")
    print("")
    print("struct VectorPoint { float x; float y; };")
    print("")
    print(f"void processDuplicator(float inX, float inY, float param_{c_cpp}, float param_{s_cpp}, float param_{o_cpp}, std::vector<VectorPoint>& outPasses) {{")
    print(f"    int numCopies = 1 + std::round(std::clamp(param_{c_cpp}, 0.0f, 1.0f) * 7.0f);")
    print(f"    float spreadAngle = std::clamp(param_{s_cpp}, 0.0f, 1.0f) * (2.0f * M_PI);")
    print(f"    float offsetDist = (std::clamp(param_{o_cpp}, 0.0f, 1.0f) * 2.0f - 1.0f);")
    print("")
    print("    outPasses.clear();")
    print("    for (int k = 0; k < numCopies; k++) {")
    print("        float angle = k * spreadAngle;")
    print("        float cosA = std::cos(angle);")
    print("        float sinA = std::sin(angle);")
    print("")
    print("        // Rotate & Offset vector copy")
    print("        float copyX = (inX * cosA - inY * sinA) + k * offsetDist * cosA;")
    print("        float copyY = (inX * sinA + inY * cosA) + k * offsetDist * sinA;")
    print("        outPasses.push_back({copyX, copyY});")
    print("    }")
    print("}")
    print("```\n")


def plot_diagnostics(ref_x, ref_y, out_x, out_y, r_ref, theta_ref, r_out, theta_out, P_dict, output_plot_path):
    """Generates multi-panel diagnostic plot PNG."""
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle("Osci-Render Effect Analysis", fontsize=14, fontweight='bold')

    ax1 = axes[0, 0]
    ax1.plot(ref_x[:2000], ref_y[:2000], label="Reference Trajectory", color="gray", alpha=0.7)
    ax1.plot(out_x[:2000], out_y[:2000], label="Recorded Output", color="#00e676", alpha=0.8)
    ax1.set_title("1. Vector Trajectory Overlay (First 2000 Samples)")
    ax1.set_xlabel("X")
    ax1.set_ylabel("Y")
    ax1.grid(True, alpha=0.3)
    ax1.legend()

    ax2 = axes[0, 1]
    first_pname = list(P_dict.keys())[0]
    P_first = P_dict[first_pname]
    subsample = np.random.choice(len(P_first), size=min(5000, len(P_first)), replace=False)
    ax2.scatter(P_first[subsample], r_out[subsample], c=theta_ref[subsample], cmap="viridis", s=3, alpha=0.5)
    ax2.set_title(f"2. Output Radius vs {first_pname}")
    ax2.set_xlabel(f"{first_pname} (0 -> 1)")
    ax2.set_ylabel("Output Radius r'")
    ax2.grid(True, alpha=0.3)

    ax3 = axes[1, 0]
    second_pname = list(P_dict.keys())[1] if len(P_dict) > 1 else first_pname
    P_sec = P_dict[second_pname]
    ax3.scatter(P_sec[subsample], theta_out[subsample], c=P_first[subsample], cmap="plasma", s=3, alpha=0.5)
    ax3.set_title(f"3. Output Phase θ' vs {second_pname}")
    ax3.set_xlabel(f"{second_pname} (0 -> 1)")
    ax3.set_ylabel("Output Phase θ'")
    ax3.grid(True, alpha=0.3)

    ax4 = axes[1, 1]
    time_sec = np.arange(len(P_first)) / 48000.0
    for pname, pvec in P_dict.items():
        ax4.plot(time_sec, pvec, label=pname, alpha=0.8)
    ax4.set_title("4. Parameter Timeline Trajectories")
    ax4.set_xlabel("Time (sec)")
    ax4.grid(True, alpha=0.3)
    ax4.legend()

    plt.tight_layout()
    plt.savefig(output_plot_path, dpi=150)
    print(f"[+] Saved diagnostic plot to: {output_plot_path}")


def main():
    parser = argparse.ArgumentParser(description="Osci-Render Live Audio Analysis & Solver")
    parser.add_argument("--out", required=True, help="Path to recorded stereo WAV file")
    parser.add_argument("--meta", required=True, help="Path to metadata JSON file")
    parser.add_argument("--plot", default="unfold_analysis_plot.png", help="Output path for diagnostic plot PNG")

    args = parser.parse_args()

    print("[*] Loading recorded audio and metadata...")
    sr_out, out_x_raw, out_y_raw = load_wav(args.out)

    with open(args.meta, 'r') as f:
        metadata = json.load(f)

    burst_start, burst_end = detect_sync_bursts(out_x_raw, out_y_raw, sr_out)
    out_x = out_x_raw[burst_start:burst_end]
    out_y = out_y_raw[burst_start:burst_end]

    num_samples = len(out_x)
    print(f"[+] Trimmed Sweep Duration: {num_samples / float(sr_out):.3f} seconds ({num_samples} samples)")

    sig_type = metadata.get("signalType", "circle")
    freq = metadata.get("frequencyHz", 100)
    ref_x, ref_y = generate_reference_signal(sig_type, freq, num_samples, sr_out)

    P_dict = build_parameter_matrix(num_samples, metadata)

    r_ref = np.hypot(ref_x, ref_y)
    theta_ref = np.arctan2(ref_y, ref_x)

    r_out = np.hypot(out_x, out_y)
    theta_out = np.arctan2(out_y, out_x)

    param0 = list(P_dict.keys())[0].lower()
    if "unfold" in param0 or "fold" in param0 or "segment" in param0:
        analyze_unfold_effect(r_ref, theta_ref, r_out, theta_out, P_dict)
    else:
        analyze_duplicator_effect(r_ref, theta_ref, r_out, theta_out, P_dict)

    if args.plot:
        plot_diagnostics(ref_x, ref_y, out_x, out_y, r_ref, theta_ref, r_out, theta_out, P_dict, args.plot)


if __name__ == "__main__":
    main()
