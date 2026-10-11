#!/usr/bin/env python3
"""Upload the existing build via ArduinoOTA; do not leak the password in argv."""
import importlib.util
import os
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
pio_home = Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio")))
script = pio_home / "packages/framework-arduinoespressif8266/tools/espota.py"
binary = root / ".pio/build/ba129/firmware.bin"
ip = os.environ.get("ESP_SWITCH_IP")
password = os.environ.get("ESP_SWITCH_OTA_PASSWORD")
if not ip or not password or not script.exists() or not binary.exists():
    raise SystemExit("Build firmware, set ESP_SWITCH_IP/ESP_SWITCH_OTA_PASSWORD, and open the idle OTA window first.")
spec = importlib.util.spec_from_file_location("espota", script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
sys.argv = [str(script), "-i", ip, "-a", password, "-f", str(binary)]
sys.exit(module.main(sys.argv[1:]))
