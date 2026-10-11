#!/usr/bin/env python3
"""Write local build credentials without printing them or committing them."""
import getpass
import json
import pathlib
import secrets

root = pathlib.Path(__file__).resolve().parents[1]
target = root / "include" / "secrets.h"
if target.exists():
    raise SystemExit("include/secrets.h already exists; edit it locally to preserve credentials.")
ssid = input("Management Wi-Fi SSID: ").strip()
if not ssid:
    raise SystemExit("SSID must not be empty")
password = getpass.getpass("Wi-Fi password: ")
device = input("Device name [esp-switch-01]: ").strip() or "esp-switch-01"
if not all(c.isascii() and (c.isalnum() or c == "-") for c in device) or len(device) > 32:
    raise SystemExit("Device name must use 1–32 ASCII letters, numbers, or hyphens")
values = {"WIFI_SSID": ssid, "WIFI_PASSWORD": password, "DEVICE_NAME": device,
          "API_TOKEN": secrets.token_hex(32), "OTA_PASSWORD": secrets.token_hex(24)}
target.write_text("#pragma once\n" + "".join(
    f"#define {key} {json.dumps(value, ensure_ascii=True)}\n" for key, value in values.items()))
target.chmod(0o600)
print("Saved include/secrets.h (ignored by Git). Store API/OTA credentials in your backend securely.")
