# ESP Switch

ESP8266 firmware for the BA129 relay board, designed to control HDMI splitter power from a PlayStation rental backend. An authenticated HTTP API manages rental sessions and manual relay control. A local timer turns the relay off when a session expires, independently of the backend connection. Firmware updates are available over Wi-Fi through a controlled OTA maintenance window.

**Current firmware: 0.1.2.** Serial flashing, Wi-Fi connectivity, session control, timer expiry, manual overrides, and OTA have been tested on a BA129 board. See the [bench test report](docs/bench-test.md) for results and limitations. HDMI signal interruption and electrical contact measurements have not yet been verified.

## Hardware

| Item | Configuration |
| --- | --- |
| Target | BA129 with ESP-12F / ESP8266EX |
| Flash | 4 MB |
| Relay | GPIO5, active HIGH |
| Indicator LED | GPIO16, active LOW |
| Serial interface | 57600 baud, 3.3 V logic |
| Flash layout | 4 MB flash / 1 MB filesystem, with space for OTA |

Use COM–NO contacts for the intended behavior: the splitter receives power during an active session and loses power when the relay is de-energized. Keep the PlayStation itself powered. A splitter must be tested to confirm that removing its external supply actually interrupts video and that it is not powered through HDMI.

### ESP32-C3 flashing bridge

The tested bridge source is included in [tools/c3_uart_bridge](tools/c3_uart_bridge). It uses a 16 KB USB receive buffer and a 4 KB transmit buffer.

| ESP32-C3 | BA129 |
| --- | --- |
| GPIO21 / TX | RX |
| GPIO20 / RX | TX |
| GND | GND |

Power the C3 through USB-C and the BA129 separately through microUSB 5 V. Disconnect all mains wiring during USB/UART testing. Do not connect the boards' 5 V pins. The bridge does not forward USB reset signals to the ESP8266; target boot/reset control is manual.

## Getting started

```sh
python3 -m venv .venv
.venv/bin/pip install platformio==6.2.0
python3 scripts/configure.py
.venv/bin/pio run -e ba129
.venv/bin/pio test -e native
```

The configuration script requests Wi-Fi credentials and generates an API token and OTA password in `include/secrets.h`. This file is excluded from Git. Store the generated credentials securely in the backend. Alternatively, copy `include/secrets.example.h` to `include/secrets.h` and configure it manually.

- API token: at least 32 characters.
- OTA password: at least 16 characters.
- Wi-Fi: 2.4 GHz; SSID matching is case-sensitive.
- Without valid configuration, the firmware leaves the relay OFF and does not enable the HTTP service.

Dependencies are pinned in [platformio.ini](platformio.ini). The firmware binary is generated at `.pio/build/ba129/firmware.bin`. Wi-Fi configuration currently happens at build time; a browser-based provisioning interface has not been implemented.

## Initial serial installation

Connect BA129 IO0 to GND, then press the BA129 RESET button. Keep the C3 running the bridge firmware.

The verified upload path uses **esptool 5.3.1**, installed separately and available on your PATH:

```sh
esptool --chip esp8266 --port /dev/cu.usbmodem11401 \
  --baud 57600 --before no-reset --after no-reset \
  write-flash --flash-mode dio --flash-freq 40m --flash-size 4MB \
  0x0 .pio/build/ba129/firmware.bin
```

Replace the serial port as needed. After a successful upload, disconnect IO0 from GND and press RESET to run the application. Serial logs at 57600 baud report the assigned IP address.

PlatformIO also provides an upload command, but its bundled esptool 3 uploader failed during stub transfer in the bench test. The older uploader has not been fixed; use the verified esptool 5 path above.

Installing this firmware replaces the previous application. The earlier 64 KB diagnostic read is not a complete backup of the 4 MB flash.

## HTTP API

The complete specification is in [docs/openapi.yaml](docs/openapi.yaml), suitable for Swagger-compatible tools and Postman.

All endpoints require:

```http
Authorization: Bearer <API_TOKEN>
Content-Type: application/json
```

Request bodies are limited to 512 bytes. The service uses HTTP without transport encryption. Keep it on a restricted management network and let the rental backend own device credentials. Customer-browser CORS, TLS, signed OTA, and fleet management are not implemented.

| Endpoint | Purpose |
| --- | --- |
| `GET /status` | Read firmware version, boot identity, relay state, control mode, session, remaining time, and network information |
| `POST /session/start` | Start a rental session with revision 1 |
| `POST /session/update` | Replace remaining time using a higher revision |
| `POST /session/stop` | End a rental session using a higher revision |
| `POST /relay/on` | Enable a timed manual override while idle |
| `POST /relay/off` | Force OFF and cancel the current rental or manual session |
| `POST /maintenance/ota` | Open or close a five-minute OTA window while idle |

### Session control

Read `/status` before issuing commands. Every mutation requires the current `boot_id`; starting a session also requires `expected_generation` from the status response. These values prevent stale commands from opening a session after another session or a device restart.

Start:

```json
{"boot_id":"BOOT_ID","expected_generation":0,"session_id":"rental-001","revision":1,"remaining_seconds":3600}
```

Update:

```json
{"boot_id":"BOOT_ID","session_id":"rental-001","revision":2,"remaining_seconds":1800}
```

Stop:

```json
{"boot_id":"BOOT_ID","session_id":"rental-001","revision":3}
```

`remaining_seconds` sets the countdown from the moment the command is received; it does not add time to the existing countdown. Valid durations are 1–86400 seconds. The backend remains responsible for billing and the authoritative session end time.

Use unique session IDs and persist each command's revision and payload before sending it. After a timeout, retry the identical payload. Identical retries do not restart the timer, even after expiry; stale revisions are rejected. Reconcile device state with the backend to account for network latency and clock drift.

Sessions are stored in RAM. After restart, the relay is OFF and the backend must read the new boot identity before restoring any still-valid rental using a new session ID and its remaining duration. Do not automatically restore expired sessions.

### Manual relay override

Read the current `boot_id` and `generation` from `/status`.

Timed ON:

```json
{"boot_id":"BOOT_ID","expected_generation":0,"request_id":"manual-001","duration_seconds":60}
```

Send this to `POST /relay/on`. The relay turns OFF automatically after 1–86400 seconds. Use a unique request ID for each operation; identical retries do not restart the timer. Manual ON is rejected while a rental or OTA maintenance window is active. Rental commands are rejected while a manual override is active.

Forced OFF:

```json
{"boot_id":"BOOT_ID","expected_generation":1}
```

Send this to `POST /relay/off` to cancel the current rental or manual session. Cancelling an active session increments the generation, so a delayed command cannot stop a newer session. Repeating the old payload then returns 409; read status to confirm the resulting state.

The status `mode` is `idle`, `rental`, or `manual`. There is no indefinite manual ON mode. Restart always leaves the relay OFF.

### Responses and state reporting

| Code | Meaning |
| --- | --- |
| 200 | Command applied, identical retry accepted, or status returned |
| 400 | Invalid JSON or command fields |
| 401 | Missing or invalid API token |
| 409 | Boot, generation, revision, session, or maintenance conflict |
| 413 | Request body exceeds the size limit |

`relay_on` reports the commanded GPIO state. It does not measure relay contacts, output voltage, or whether the television is displaying an image.

### Command-line client

```sh
export ESP_SWITCH_TOKEN='YOUR_LOCAL_API_TOKEN'
python3 scripts/client.py http://192.168.1.100 status
python3 scripts/client.py http://192.168.1.100 start --session rental-001 --revision 1 --seconds 3600
python3 scripts/client.py http://192.168.1.100 update --session rental-001 --revision 2 --seconds 1800
python3 scripts/client.py http://192.168.1.100 stop --session rental-001 --revision 3
python3 scripts/client.py http://192.168.1.100 relay-on --request-id manual-001 --seconds 60
python3 scripts/client.py http://192.168.1.100 relay-off
```

Replace the example IP with the device address. The helper reads status before issuing commands. A production backend must persist retry payloads and revisions and handle conflicts explicitly.

## OTA updates

The first installation requires serial flashing. Subsequent updates can use Wi-Fi without the C3 bridge or an IO0 jumper.

1. Build the new firmware with the device's local configuration.
2. Verify that the device is idle, then open the maintenance window.
3. Upload using the local OTA password.
4. Wait for restart and confirm the firmware version and relay state.

```sh
.venv/bin/pio run -e ba129
export ESP_SWITCH_TOKEN='YOUR_LOCAL_API_TOKEN'
export ESP_SWITCH_IP=192.168.1.100
export ESP_SWITCH_OTA_PASSWORD='YOUR_LOCAL_OTA_PASSWORD'
.venv/bin/python scripts/client.py "http://$ESP_SWITCH_IP" ota-open
.venv/bin/python scripts/ota.py
.venv/bin/python scripts/client.py "http://$ESP_SWITCH_IP" status
```

The maintenance window lasts five minutes and blocks new sessions. To close it early:

```sh
.venv/bin/python scripts/client.py "http://$ESP_SWITCH_IP" ota-close
```

OTA is rejected while a session is active. After a successful update, the device restarts with the relay OFF. Future firmware must retain OTA support. The computer and device need bidirectional network connectivity for ArduinoOTA, and the image must fit the available update space.

Do not interrupt power during an update. Automatic rollback is not guaranteed; an unusable application or lost Wi-Fi configuration may require serial recovery. OTA password authentication does not provide signed-image verification.

### Changing Wi-Fi credentials

Edit `WIFI_SSID` and `WIFI_PASSWORD` in the ignored `include/secrets.h`, preserving the API and OTA credentials. Build and upload through the existing network; after restart, the device connects using the new configuration. If the old network is no longer reachable, update through the serial bridge.

## Diagnostics and validation

Serial diagnostics run at 57600 baud. At startup, the device scans for up to 50 access points and reports SSID, RSSI in dBm, channel, BSSID, encryption code, and hidden-network status. Hidden SSIDs may remain empty.

Every ten seconds, the device reports connection status, IP, and RSSI. While disconnected, it repeats the cached startup scan. Sending `S` over serial also prints the cached report; it does not trigger a new scan. Scanning occurs only at startup with the relay OFF. Passwords and tokens are not printed.

Six native tests cover forced cancellation, retries, stale revisions, expiry, session conflicts, duration limits, and `millis()` rollover. Device testing covers HTTP control, manual override, and OTA; see [docs/bench-test.md](docs/bench-test.md).

Remaining validation includes contact/output measurements, actual HDMI interruption, Wi-Fi-loss behavior, startup GPIO transients, and cutoff timing under slow HTTP clients. The synchronous HTTP server can delay timer processing; behavior before `setup()` is not guaranteed by application code.

Reference: [ESP8266 Arduino OTA documentation](https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html).
