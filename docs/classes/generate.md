# Functional Class Directives: Generate

This document defines the mandatory specifications, standards, and architecture for modules belonging to the **Generate** class (oscillators, chaos/noise engines, pattern/function generators, audio/CV sources).

---

## 1. Generator Output Voltage Standards (Mandatory)

All modules belonging to the **Generate** class MUST implement user-selectable output voltage range standards via the right-click context menu:

### Supported Standards
1. `0-1V` (Unipolar $0\text{ V}$ to $+1\text{ V}$)
2. `+/-1V` (Bipolar $-1\text{ V}$ to $+1\text{ V}$)
3. `0-5V` (Unipolar $0\text{ V}$ to $+5\text{ V}$)
4. `+/-5V` (Bipolar $-5\text{ V}$ to $+5\text{ V}$)
5. `0-10V` (Unipolar $0\text{ V}$ to $+10\text{ V}$)
6. `+/-10V` (Bipolar $-10\text{ V}$ to $+10\text{ V}$)

### Persistence & Architecture Requirements
- Store the selected standard as an enum (`VoltageRange`) in the module struct.
- Serialize and restore via `dataToJson()` and `dataFromJson()` under the key `"voltageRange"`.
- Provide a helper method `getRangeVoltages(float& vMin, float& vMax)` in the module so generated outputs scale accurately to the active bounds.
- Expose the selection in `ModuleWidget::appendContextMenu()` under a `"Voltage Range"` label with checkmarks for each mode.

---

## 2. Reference Implementation Pattern

```cpp
// 1. In Module struct:
enum VoltageRange {
    RANGE_UNIPOLAR_1V = 0,
    RANGE_BIPOLAR_1V,
    RANGE_UNIPOLAR_5V,
    RANGE_BIPOLAR_5V,
    RANGE_UNIPOLAR_10V,
    RANGE_BIPOLAR_10V,
    NUM_RANGES
};
VoltageRange voltageRange = RANGE_BIPOLAR_5V; // or module-appropriate default

void getRangeVoltages(float& vMin, float& vMax) const {
    switch (voltageRange) {
        case RANGE_UNIPOLAR_1V:  vMin =  0.0f; vMax =  1.0f; break;
        case RANGE_BIPOLAR_1V:   vMin = -1.0f; vMax =  1.0f; break;
        case RANGE_UNIPOLAR_5V:  vMin =  0.0f; vMax =  5.0f; break;
        case RANGE_BIPOLAR_5V:   vMin = -5.0f; vMax =  5.0f; break;
        case RANGE_UNIPOLAR_10V: vMin =  0.0f; vMax = 10.0f; break;
        case RANGE_BIPOLAR_10V:
        default:                 vMin = -10.0f; vMax = 10.0f; break;
    }
}

json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "voltageRange", json_integer((int)voltageRange));
    return rootJ;
}

void dataFromJson(json_t* rootJ) override {
    json_t* vrJ = json_object_get(rootJ, "voltageRange");
    if (vrJ) voltageRange = (VoltageRange)json_integer_value(vrJ);
}

// 2. In ModuleWidget::appendContextMenu:
void appendContextMenu(Menu* menu) override {
    auto* mod = dynamic_cast<MyModule*>(this->module);
    if (!mod) return;

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("Voltage Range"));

    const char* rangeLabels[] = {
        "0-1V (0V to +1V)",
        "+/-1V (-1V to +1V)",
        "0-5V (0V to +5V)",
        "+/-5V (-5V to +5V)",
        "0-10V (0V to +10V)",
        "+/-10V (-10V to +10V)"
    };

    for (int i = 0; i < MyModule::NUM_RANGES; i++) {
        MyModule::VoltageRange mode = (MyModule::VoltageRange)i;
        menu->addChild(createCheckMenuItem(rangeLabels[i], "",
            [=]() { return mod->voltageRange == mode; },
            [=]() { mod->voltageRange = mode; }
        ));
    }
}
```

---

## 3. Real-Time DSP Directives for Generators

1. **Bandlimiting & Anti-Aliasing**: Audio-rate oscillators must implement anti-aliasing (e.g. PolyBLEP, minBLEP, oversampling, or bandlimited wavetables) when producing hard transitions (square, saw, pulse, sync).
2. **Phase Accumulation**: Use normalized phase $[0.0, 1.0)$ with wraparound handling.
3. **Real-Time Safety**: Zero dynamic memory allocation (`malloc`, `new`, `std::vector::push_back`) or file I/O in `process()`.
4. **Tag in plugin.json**: Ensure `"Generate"` is added to the module's `"tags"` array in `plugin.json`.
