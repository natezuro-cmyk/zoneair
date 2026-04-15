# Zone Air — TCL Mini-Split WiFi Control (Design Spec)

**Date:** 2026-04-15
**Owner:** Ben
**Status:** Approved design, ready for implementation plan

## 1. Goal

Replace the stock TCL WiFi dongle on TCL-built mini-split heat pumps with a Zone Air ESP32-S3 SuperMini and ship a branded iOS + Android app that controls one or many units locally over the user's home WiFi.

TCL is the OEM behind several rebadges (Della and others); v1 targets TCL-built units only. No multi-protocol auto-detection. No cloud. No accounts.

## 2. Why this exists

A prior attempt got a working ESP web server (v3) but every attempt to move the UI off the ESP — to a hosted page, to an app — broke. Root cause: same-origin assumptions. A page served from the ESP can hit the ESP's HTTP API freely; a page served from anywhere else hits CORS, mixed-content (HTTPS page → HTTP ESP), or can't discover the ESP's IP. This spec fixes those root causes explicitly.

## 3. Non-goals (v1)

- Away-from-home control (no cloud relay).
- Multi-home / multi-user accounts.
- Pioneer or other non-TCL protocols.
- Apple Home / Google Home / Matter integration (separate project).
- Web-only PWA shipping (App Store presence is required).

## 4. Architecture

Three pieces, all under Zone Air's control:

1. **ESP32-S3 SuperMini firmware** — one per AC unit. UART to AC main board. BLE for first-time WiFi provisioning. mDNS + HTTP + WebSocket on home WiFi for runtime control.
2. **Zone Air app** — one Capacitor + React + TypeScript codebase. Runs in a browser during development. Ships to iOS App Store and Google Play.
3. **No backend.** Everything is local-network. Phone talks directly to ESPs.

## 5. Onboarding flow (BLE provisioning)

Chosen over SoftAP captive portal because it eliminates the iOS WiFi-switching pain that historically derails this UX.

1. ESP boots. No saved WiFi creds → enters BLE provisioning mode using Espressif's `wifi_provisioning` library. Advertises as `ZoneAir-XXXX` (last 4 hex of MAC).
2. User opens app → "Add Unit" → app scans BLE → lists nearby `ZoneAir-*` devices.
3. User picks one. App connects over BLE.
4. App prompts user to name the unit (e.g., "Living Room") and pick home WiFi SSID + password.
5. App writes creds over BLE characteristic via Espressif's `wifi_prov_mgr` protocol, encrypted with a per-device PoP code printed on the unit label.
6. ESP saves creds to NVS, reboots, joins home WiFi, advertises mDNS as `zoneair-<slug>.local`.
7. App scans mDNS, confirms unit online, persists to local unit list (Capacitor Preferences).
8. Unit appears in main UI with full control.

Factory reset (long-press boot button, pattern carried over from Pioneer firmware) wipes NVS and re-enters BLE provisioning mode.

## 6. Runtime control (app ↔ ESP after onboarding)

**Discovery:** mDNS hostname (`zoneair-<slug>.local`), with cached IP fallback for routers that block mDNS (common on mesh setups).

**Two channels per unit:**

- **HTTP REST** for one-off actions and initial state load.
  - `GET /state` → JSON: `{mode, setpoint, fan, indoor_temp, power, online}`.
  - `POST /command` → JSON body: `{mode?, setpoint?, fan?, power?}`. Idempotent. Fire-and-forget at the UART layer.
- **WebSocket** for live state push.
  - `ws://<host>/ws` per visible unit.
  - ESP pushes a state frame on every AC state change; heartbeat every 3 s otherwise.
  - Eliminates the "UI shows wrong state" failure mode.

**CORS:** ESP returns `Access-Control-Allow-Origin: *` on every response. Required because the app is *always* served from a different origin (Vite dev server in browser; `capacitor://localhost` in native). Safe — local network only.

**Mixed content:** Dev = `http://localhost`, native = `capacitor://localhost`. Both can call `http://` ESPs without browser blocks. A pure HTTPS PWA could not — that is exactly why prior browser attempts failed.

**Multi-unit:** App opens one WebSocket per visible unit, closes it when navigating away. Battery-friendly.

## 7. Firmware structure (ESP32-S3 SuperMini)

```
zoneair-firmware/
├── zoneair-firmware.ino
├── protocol/
│   └── tcl.{h,cpp}            # protocol logic lifted from lNikazzzl/tcl_ac_esphome
├── transport/
│   ├── http_server.{h,cpp}    # REST + permissive CORS
│   └── ws_server.{h,cpp}      # WebSocket state push
├── provisioning/
│   └── ble_prov.{h,cpp}       # wraps Espressif wifi_provisioning
├── discovery/
│   └── mdns.{h,cpp}           # advertises zoneair-<slug>.local
└── references/
    └── lNikazzzl-tcl_ac_esphome/   # git submodule for diff/audit
```

Reused proven patterns from Pioneer firmware (validated on real hardware):

- 300 ms query response timeout; set commands fire-and-forget.
- 3 s state poll interval; 15 s heartbeat backstop.
- Boot-button long-press → NVS wipe → re-enter BLE provisioning.
- Hardware watchdog feed in main loop.
- WBR3 module must be physically disconnected before installing the ESP — bus contention causes total UART failure. Document in install guide.

The TCL protocol code is lifted (not forked) from `https://github.com/lNikazzzl/tcl_ac_esphome` into plain C++ classes that drop into the Arduino structure. The original repo is vendored under `references/` as a submodule for audit and future diffs.

## 8. App structure (Capacitor + React + TypeScript)

```
zoneair-app/
├── src/
│   ├── pages/
│   │   ├── UnitList.tsx
│   │   ├── UnitControl.tsx
│   │   └── AddUnit.tsx
│   ├── lib/
│   │   ├── ble.ts              # @capacitor-community/bluetooth-le wrapper
│   │   ├── provisioning.ts     # Espressif wifi_prov protocol over BLE
│   │   ├── discovery.ts        # @capacitor-community/zeroconf
│   │   ├── unitClient.ts       # per-unit HTTP + WebSocket
│   │   └── storage.ts          # Capacitor Preferences
│   ├── state/
│   │   └── unitsStore.ts       # Zustand
│   └── App.tsx
├── ios/                         # `npx cap add ios`
├── android/                     # `npx cap add android`
├── capacitor.config.ts
├── package.json
└── vite.config.ts
```

**Stack:** Vite + React + TypeScript + Capacitor + Tailwind + Zustand. No Redux. No backend SDK.

**Native plugins:** `@capacitor-community/bluetooth-le`, `@capacitor-community/zeroconf`.

**Permissions:**

- iOS: `NSBluetoothAlwaysUsageDescription`, `NSLocalNetworkUsageDescription`, `NSBonjourServices: ["_zoneair._tcp"]`.
- Android: `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT`, `ACCESS_FINE_LOCATION`, `INTERNET`.

**Dev loop:** `npm run dev` against a real ESP on the LAN → `npx cap sync` → `npx cap run ios|android` to validate native.

## 9. Error handling and state sync

- **Unit offline:** WebSocket close + HTTP retry with exponential backoff (1 s → 2 s → 4 s, capped at 30 s). UI greys the card, shows "Offline."
- **mDNS fails:** Fall back to last known IP from storage. If both fail, prompt user to power-cycle the unit.
- **BLE provisioning timeout:** If ESP fails to join home WiFi within 30 s of receiving creds, surface "Wrong password?" and let user retry without re-pairing BLE.
- **Command rejected by AC:** ESP returns 200 (matches fire-and-forget UART semantics). Next state push is the source of truth. UI shows requested state optimistically, reverts after 5 s if AC didn't honor it.
- **Firmware crash:** Hardware watchdog reboots; app's WebSocket reconnect handles it transparently.

**State authority:** ESP's last-known AC state, pushed over WebSocket. App never trusts a command "worked" without seeing it reflected in a state push. Optimistic UI is allowed but always reconciles within one poll cycle (3 s).

## 10. Testing

- **Firmware unit tests:** `tcl.cpp` encode/decode against fixture byte arrays lifted from the lNikazzzl repo's known-good captures.
- **Firmware integration:** small Python TCL UART simulator (mirroring `pioneer-matter/tools/mcb_simulator.py`) so firmware can run without a real AC.
- **App unit tests:** Vitest for everything in `lib/`. Capacitor web fallbacks mock BLE/mDNS during browser dev.
- **End-to-end:** real ESP + real AC, manual smoke test checklist run before every release build.

## 11. References

- TCL UART protocol: `https://github.com/lNikazzzl/tcl_ac_esphome` (locally cloned at `/Users/Ben/esphome/.esphome/external_components/699f8438/`).
- Pioneer UART protocol (not used in v1, kept for context): `https://github.com/mikesmitty/esphome-components` (locally cloned at `/Users/Ben/esphome/.esphome/external_components/f725b37d/`).
- Espressif WiFi provisioning library: part of ESP-IDF `wifi_provisioning` component.
- Prior project context: `/Users/Ben/pioneer-matter/` (Pioneer firmware shipping today; source of validated timing and reset patterns).
