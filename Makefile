# Set RACK_DIR to the Rack SDK path
RACK_DIR ?= G:/dupe/work/rack/Rack-SDK

# Add source files
SOURCES += src/plugin.cpp
SOURCES += src/Chromance.cpp
SOURCES += src/Ants.cpp
SOURCES += src/HsvRgb.cpp
SOURCES += src/OklchRgb.cpp
SOURCES += src/Rgb.cpp
SOURCES += src/Instability.cpp
SOURCES += src/Scale.cpp
SOURCES += src/Position.cpp
SOURCES += src/Rotate.cpp
SOURCES += src/Spin.cpp
SOURCES += src/Fold.cpp
SOURCES += src/Stretch.cpp
SOURCES += src/Shear.cpp
SOURCES += src/Smooth.cpp
SOURCES += src/Wiggle.cpp
SOURCES += src/Crest.cpp
SOURCES += src/Quake.cpp
SOURCES += src/Steps.cpp
SOURCES += src/Copycat.cpp
SOURCES += src/Petals.cpp
SOURCES += src/Knots.cpp
SOURCES += src/Vortex.cpp
SOURCES += src/Trough.cpp
SOURCES += src/Switch.cpp
SOURCES += src/Route.cpp
SOURCES += src/SumMult.cpp
SOURCES += src/SwitchXL.cpp
SOURCES += src/RouteXL.cpp
SOURCES += src/SumMixXL.cpp
SOURCES += src/Andxy.cpp
SOURCES += src/Orxy.cpp
SOURCES += src/Xorxy.cpp
SOURCES += src/ChopXL.cpp
SOURCES += src/Rescale.cpp
SOURCES += src/Bleed.cpp

# Add distributable files
DISTRIBUTABLES += res
DISTRIBUTABLES += plugin.json

RACK_USER_DIR ?= C:/Users/Pat/AppData/Local/Rack2/plugins-win-x64/Garmire
RACK_PLUGINS_DIR ?= C:/Users/Pat/AppData/Local/Rack2/plugins-win-x64

# Include the Rack SDK build system
include $(RACK_DIR)/plugin.mk

install: dist
	@mkdir -p "$(RACK_USER_DIR)"
	@cp -r res "$(RACK_USER_DIR)/" 2>/dev/null || true
	@cp plugin.json "$(RACK_USER_DIR)/" 2>/dev/null || true
	@cp plugin.dll "$(RACK_USER_DIR)/" 2>/dev/null || echo "[!] Notice: VCV Rack is currently running and locking plugin.dll. Please close VCV Rack to complete copying plugin.dll."
	@cp dist/*.vcvplugin "$(RACK_PLUGINS_DIR)/" 2>/dev/null || true
	@echo "Plugin install process finished for $(RACK_USER_DIR) and $(RACK_PLUGINS_DIR)"
