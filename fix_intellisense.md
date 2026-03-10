# ESPHome IDE Fix: Restoring IntelliSense & Codegen

Use these steps if "esphome.codegen" or C++ headers (like NimBLE, stdlib, or ESP-IDF)
show red squiggles in VS Code while using a Dev Container.

## 1. Python / Codegen Fix
Ensure the IDE is looking at the Python environment inside the container.
- Open Command Palette (Ctrl+Shift+P).
- Select: "Python: Select Interpreter".
- Choose: `/workspaces/esphome.official/.venv/bin/python` (or your specific container venv).

## 2. C++ Header / IntelliSense Fix
Run these commands in the VS Code terminal inside the container to generate
absolute paths for the C++ engine.

# Step A: Force ESPHome to generate the PlatformIO project files
esphome compile config/your_device.yaml --only-generate

# Step B: Enter the generated build folder
cd config/.esphome/build/your_device_name/

# Step C: Use PlatformIO to generate the VS Code metadata
pio init --ide vscode

# Step D: Move the generated config to your project root
cp .vscode/c_cpp_properties.json /workspaces/esphome.official/.vscode/

## 3. Refreshing the IDE
- Open Command Palette (Ctrl+Shift+P).
- Run: "C/C++: Reset IntelliSense Database".
- Ensure the PlatformIO extension is "Disabled (Workspace)" so it doesn't overwrite your file.


## Or add a task (.vscode/tasks.json):
~~~
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "ESPHome: Fix IntelliSense",
      "type": "shell",
      "command": "esphome compile config/${input:deviceName}.yaml --only-generate && cd config/.esphome/build/${input:deviceName}/ && pio init --ide vscode && cp .vscode/c_cpp_properties.json ${workspaceFolder}/.vscode/",
      "problemMatcher": [],
      "group": "build",
      "presentation": {
        "reveal": "always",
        "panel": "shared"
      }
    }
  ]
}
~~~
