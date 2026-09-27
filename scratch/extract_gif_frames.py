import os
from PIL import Image

def extract_frames():
    gif_path = "26-09-26_osci-render_(19h09m15s).gif"
    out_dir = "scratch/gif_frames"
    os.makedirs(out_dir, exist_ok=True)
    
    img = Image.open(gif_path)
    n_frames = img.n_frames
    print(f"Total frames in GIF: {n_frames}")
    
    # Extract 8 evenly spaced frames across the animation
    indices = [int(i * (n_frames - 1) / 7) for i in range(8)]
    for idx in indices:
        img.seek(idx)
        frame = img.convert("RGB")
        frame_path = os.path.join(out_dir, f"frame_{idx:03d}.png")
        frame.save(frame_path)
        print(f"Saved {frame_path}")

if __name__ == "__main__":
    extract_frames()
