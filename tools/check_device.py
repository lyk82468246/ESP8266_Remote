"""Protocol v1 checks against a real board; explicitly turns the LED on then off."""
import argparse
import http.client
import json
import time
import uuid
from urllib.parse import urlparse


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="http://192.168.4.1")
    parser.add_argument("--resource", default="led.main")
    parser.add_argument("--cycles", type=int, default=10, help="10 cycles = 20 state changes")
    args = parser.parse_args()
    address = urlparse(args.host)
    if address.scheme != "http" or not address.hostname or address.path not in ("", "/"):
        parser.error("--host must be an HTTP origin, e.g. http://192.168.4.1")
    if args.cycles < 1:
        parser.error("--cycles must be positive")
    if not args.resource or len(args.resource) > 48 or any(
        c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-" for c in args.resource
    ):
        parser.error("invalid resource ID")

    def call(method, path, body=None, media="application/json"):
        connection = http.client.HTTPConnection(address.hostname, address.port or 80, timeout=5)
        wire = None if body is None else body.encode("utf-8")
        started = time.monotonic()
        try:
            connection.request(method, path, body=wire, headers={"Content-Type": media})
            response = connection.getresponse()
            data = json.loads(response.read(16385).decode("utf-8"))
            assert data["protocolVersion"] == 1
            assert response.getheader("Content-Type", "").startswith("application/json")
            assert response.getheader("Cache-Control") == "no-store"
            print(f"{method} {path}: {response.status}, {(time.monotonic() - started)*1000:.1f} ms")
            return response.status, data
        finally:
            connection.close()

    status, device = call("GET", "/api/v1/device")
    assert status == 200
    boot = device["bootId"]
    capabilities = {r["id"]: r for r in device["resources"]}
    assert args.resource in capabilities
    properties = {p["id"]: p for p in capabilities[args.resource]["properties"]}
    assert properties["on"]["type"] == "boolean" and properties["on"]["writable"]
    endpoint = f"/api/v1/resources/{args.resource}/properties/on"

    def state():
        status, snapshot = call("GET", "/api/v1/state")
        assert status == 200 and snapshot["bootId"] == boot and snapshot["deviceId"] == device["deviceId"]
        resource = next(r for r in snapshot["resources"] if r["id"] == args.resource)
        assert resource["quality"]["on"] == "good"
        return resource["values"]["on"], snapshot["revision"]

    def body(value, expected=boot):
        return json.dumps({"requestId": str(uuid.uuid4()), "expectedBootId": expected, "value": value})

    def set_value(value):
        previous, revision = state()
        request = body(value)
        status, result = call("PUT", endpoint, request)
        assert status == 200 and result["value"] is value
        assert result["requestId"] == json.loads(request)["requestId"]
        assert result["bootId"] == boot and result["deviceId"] == device["deviceId"]
        assert result["revision"] == revision + (previous != value)
        assert state() == (value, result["revision"])

    def rejected(request, status, code, media="application/json"):
        before = state()
        actual, result = call("PUT", endpoint, request, media)
        assert actual == status and result["error"]["code"] == code
        assert state() == before

    set_value(False)
    for _ in range(args.cycles):
        set_value(True)
        set_value(True)  # Idempotent target, revision must stay unchanged.
        set_value(False)
    for value in ("true", 1, 1.5, None, [], {}):
        rejected(body(value), 422, "INVALID_VALUE")
    rejected(body(True, "boot-old"), 409, "DEVICE_RESTARTED")
    rejected("{", 400, "INVALID_REQUEST")
    rejected(body(True) + "junk", 400, "INVALID_REQUEST")
    rejected(body(True), 415, "UNSUPPORTED_MEDIA_TYPE", "text/plain")
    rejected(" " * 1025, 413, "PAYLOAD_TOO_LARGE")
    print(f"PASS: {args.cycles*2} state changes, idempotency and error checks. Output is off.")


if __name__ == "__main__":
    main()
