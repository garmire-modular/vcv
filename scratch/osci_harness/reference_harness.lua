-- Osci-Render Multi-Shape Reference Generator & Sync Marker Script
-- =================================================================
-- Includes 4 reference trajectories: Circle, Square, Ramp, Lissajous (3:2)
-- Controlled via Lua Slider A (or MIDI CC mapped to Slider A).
-- Automatically resets phase and emits a 1kHz Sync Beep on shape change.

if not initialized then
    sample_index = 0
    burst_samples = math.floor(0.1 * sample_rate) -- 100ms sync burst
    freq = frequency or 100
    active_shape = -1
    burst_counter = 0
    initialized = true
end

-- Determine shape from slider_a (0.0 to 1.0)
local shape_val = slider_a or 0.0
local current_shape = 0
if shape_val < 0.25 then
    current_shape = 0 -- Circle
elseif shape_val < 0.50 then
    current_shape = 1 -- Square
elseif shape_val < 0.75 then
    current_shape = 2 -- Ramp
else
    current_shape = 3 -- Lissajous (3:2)
end

-- Detect shape change and trigger fresh sync burst
if current_shape ~= active_shape then
    active_shape = current_shape
    burst_counter = burst_samples -- Arm 100ms sync burst
    sample_index = 0 -- Reset trajectory time
end

-- 1. Sync Burst Marker (First 100ms on initial start or shape change)
if burst_counter > 0 then
    burst_counter = burst_counter - 1
    local burst_t = (burst_samples - burst_counter) / sample_rate
    local syncVal = 0.9 * math.sin(2 * math.pi * 1000 * burst_t)
    return { syncVal, syncVal }
end

-- 2. Main Reference Trajectories
sample_index = sample_index + 1
local t = sample_index / sample_rate
local omega = 2 * math.pi * freq
local x, y = 0.0, 0.0

if active_shape == 0 then
    -- 1. Unit Circle
    x = math.cos(omega * t)
    y = math.sin(omega * t)

elseif active_shape == 1 then
    -- 2. Square Bounding Box
    local phase = (freq * t) % 1.0
    if phase < 0.25 then
        x = -1.0 + 8.0 * phase
        y = 1.0
    elseif phase < 0.50 then
        x = 1.0
        y = 1.0 - 8.0 * (phase - 0.25)
    elseif phase < 0.75 then
        x = 1.0 - 8.0 * (phase - 0.50)
        y = -1.0
    else
        x = -1.0
        y = -1.0 + 8.0 * (phase - 0.75)
    end

elseif active_shape == 2 then
    -- 3. Diagonal Ramp Line
    local phase = (freq * t) % 1.0
    local tri = phase < 0.5 and (4.0 * phase - 1.0) or (3.0 - 4.0 * phase)
    x = tri
    y = tri

elseif active_shape == 3 then
    -- 4. Lissajous (3:2 Ratio)
    x = math.sin(3 * omega * t)
    y = math.sin(2 * omega * t + math.pi / 4)
end

return { x, y }
