#!/usr/bin/env python3
"""Small backend/admin client; token supplied through environment, not argv."""
import argparse
import json
import os
import urllib.error
import urllib.request

def request(base, path, payload=None):
    token = os.environ.get("ESP_SWITCH_TOKEN")
    if not token:
        raise SystemExit("Set ESP_SWITCH_TOKEN in your environment")
    req = urllib.request.Request(base.rstrip("/") + path,
        data=None if payload is None else json.dumps(payload).encode(),
        headers={"Authorization": "Bearer " + token, "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as res:
            return json.load(res)
    except urllib.error.HTTPError as err:
        raise SystemExit(f"HTTP {err.code}: {err.read().decode()}") from err

def main():
    p = argparse.ArgumentParser()
    p.add_argument("base", help="http://device-IP")
    p.add_argument("action", choices=["status", "start", "update", "stop", "ota-open", "ota-close"])
    p.add_argument("--session")
    p.add_argument("--revision", type=int)
    p.add_argument("--seconds", type=int)
    args = p.parse_args()
    state = request(args.base, "/status")
    if args.action == "status":
        print(json.dumps(state, indent=2)); return
    payload = {"boot_id": state["boot_id"]}
    if args.action.startswith("ota-"):
        payload["enabled"] = args.action == "ota-open"
        path = "/maintenance/ota"
    else:
        if not args.session or args.revision is None:
            p.error("--session and --revision required")
        payload.update(session_id=args.session, revision=args.revision)
        if args.action != "stop":
            if args.seconds is None: p.error("--seconds required")
            payload["remaining_seconds"] = args.seconds
        if args.action == "start": payload["expected_generation"] = state["generation"]
        path = "/session/" + args.action
    print(json.dumps(request(args.base, path, payload), indent=2))

if __name__ == "__main__":
    main()
