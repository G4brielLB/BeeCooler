Import("env")

import re
from pathlib import Path


REQUIRED_MAC_KEYS = ("SENSOR_MAC", "GATEWAY_MAC")
MAC_PATTERN = re.compile(r"(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}")


def load_simple_env(path):
    values = {}
    with path.open("r", encoding="utf-8") as env_file:
        for line_number, raw_line in enumerate(env_file, start=1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line:
                raise RuntimeError(
                    f"Invalid .env entry on line {line_number}: expected KEY=value"
                )
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


project_dir = Path(env.subst("$PROJECT_DIR"))
env_path = project_dir / ".env"
if not env_path.is_file():
    raise RuntimeError(
        "Missing project-root .env. Copy .env.example to .env and configure "
        "SENSOR_MAC and GATEWAY_MAC."
    )

configuration = load_simple_env(env_path)
mac_bytes = {}
for key in REQUIRED_MAC_KEYS:
    value = configuration.get(key, "")
    if not MAC_PATTERN.fullmatch(value):
        raise RuntimeError(
            f"Missing or invalid {key} in .env; expected six hexadecimal bytes "
            "in AA:BB:CC:DD:EE:FF form."
        )
    mac_bytes[key] = [int(part, 16) for part in value.split(":")]

definitions = []
for key, address in mac_bytes.items():
    for index, byte in enumerate(address):
        definitions.append((f"{key}_B{index}", byte))

env.Append(CPPDEFINES=definitions)
print("Loaded validated ESP-NOW MAC configuration from project-root .env.")
