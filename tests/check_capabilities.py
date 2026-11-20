"""Validate native firmware responses against the pinned Android_WIFI JSON Schema."""
import argparse
import json
import math
from pathlib import Path

from jsonschema import Draft202012Validator


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schema", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, default=Path(".tools/tests"))
    parser.add_argument("--tlc549", action="store_true", help="validate the paused TLC549 experiment fixtures")
    args = parser.parse_args()
    schema = json.loads(args.schema.read_text(encoding="utf-8"))
    Draft202012Validator.check_schema(schema)
    description, state, receipt = (
        json.loads((args.fixtures / name).read_text(encoding="utf-8"))
        for name in ("device.json", "state.json", "receipt.json")
    )
    Draft202012Validator(schema).validate(description)
    for key in ("protocolVersion", "deviceId", "bootId", "capabilitiesRevision"):
        assert description[key] == state[key], key
    assert type(description["capabilitiesRevision"]) is int
    assert description["capabilitiesRevision"] >= 0
    assert 0 <= state["revision"] < receipt["revision"]
    assert receipt["bootId"] == state["bootId"] and receipt["deviceId"] == state["deviceId"]
    assert type(receipt["value"]) is bool
    resources = {resource["id"]: resource for resource in description["resources"]}
    assert len(resources) == len(description["resources"])
    snapshots = [state] + [json.loads((args.fixtures / name).read_text(encoding="utf-8"))
                          for name in ("state-analog-good.json", "state-analog-above-range.json")]
    for snapshot in snapshots:
        for key in ("protocolVersion", "deviceId", "bootId", "capabilitiesRevision"):
            assert snapshot[key] == description[key]
        assert set(resources) == {resource["id"] for resource in snapshot["resources"]}
        for resource in snapshot["resources"]:
            properties = {prop["id"]: prop for prop in resources[resource["id"]]["properties"]}
            assert set(properties) == set(resource["values"]) == set(resource["quality"])
            for key, value in resource["values"].items():
                prop = properties[key]
                if value is None:
                    assert not prop["writable"]
                    assert resource["quality"][key] == "unavailable"
                else:
                    if prop["type"] == "boolean":
                        assert type(value) is bool
                    elif prop["type"] == "integer":
                        assert type(value) is int
                        assert prop["min"] <= value <= prop["max"]
                        assert (value - prop["min"]) % prop["step"] == 0
                    else:
                        assert prop["type"] == "number" and type(value) in (int, float) and math.isfinite(value)
                        assert prop["min"] <= value <= prop["max"]
                    assert resource["quality"][key] == "good"
            if resource["id"] == "sensor.pot":
                raw, voltage = resource["values"]["raw"], resource["values"]["voltage"]
                if voltage is not None:
                    assert 0 <= raw <= 1024
                    # C++ round uses half-away-from-zero, unlike Python round's ties-to-even.
                    expected = math.floor(raw * 3.3 / 1024 * 1000 + 0.5) / 1000
                    assert abs(voltage - expected) < 1e-9
    if args.tlc549:
        light_description, light_state = (
            json.loads((args.fixtures / name).read_text(encoding="utf-8"))
            for name in ("device-tlc549.json", "state-tlc549.json")
        )
        Draft202012Validator(schema).validate(light_description)
        for key in ("protocolVersion", "deviceId", "bootId", "capabilitiesRevision"):
            assert light_state[key] == light_description[key], key
        assert light_description["capabilitiesRevision"] == 3
        historical_base = {"led.main", "led.red", "led.green", "pwm.red", "pwm.green", "sensor.pot"}
        assert {r["id"] for r in light_description["resources"]} == historical_base | {"sensor.light"}
        assert {r["id"] for r in light_state["resources"]} == historical_base | {"sensor.light"}
        light = next(r for r in light_state["resources"] if r["id"] == "sensor.light")
        assert light["values"] == {"raw": 255, "voltage": 3.287}
        assert light["quality"] == {"raw": "good", "voltage": "good"}
    fixture_names = ["device.json", "state.json", "receipt.json", "state-analog-good.json",
                     "state-analog-above-range.json"]
    if args.tlc549:
        fixture_names.extend(["device-tlc549.json", "state-tlc549.json"])
    for name in fixture_names:
        assert (args.fixtures / name).stat().st_size <= 4096
    print("PASS: actual firmware device/state/receipt match upstream schema and identity/version rules")


if __name__ == "__main__":
    main()
