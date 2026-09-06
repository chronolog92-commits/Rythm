#!/usr/bin/env python3
"""PlatformIO ststm32-Plattform für RHYTHM einrichten.

Einmalig nach dem PlatformIO-Install ausführen (bzw. wenn sich die
Plattform-Version ändert). Macht drei Dinge:

  1. Kopiert builder/frameworks/bar.py (RHYTHM-Framework) in die Plattform.
  2. Registriert "bar" in platform.json.
  3. Patcht boards/nucleo_f767zi.json ('bar' ins frameworks-Array).

Nach dem Setup:  python -m platformio run
"""
import json
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
PLATFORM = Path.home() / ".platformio/platforms/ststm32"
BAR_SRC = HERE / "bar.py"


def main() -> int:
    if not PLATFORM.exists():
        print(f"Plattform nicht gefunden: {PLATFORM}")
        print("Zuerst aus firmware/ ausführen: python -m platformio run")
        return 1

    # 1) bar.py kopieren
    dst = PLATFORM / "builder/frameworks/bar.py"
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(BAR_SRC, dst)
    print(f"[1/3] {dst}")

    # 2) platform.json: "bar" registrieren
    pj = PLATFORM / "platform.json"
    data = json.loads(pj.read_text())
    fws = data.setdefault("frameworks", {})
    if "bar" not in fws:
        fws["bar"] = {
            "script": "builder/frameworks/bar.py",
            "description": "Bare metal build using repo-internal vendor/ sources (RHYTHM)",
            "title": "Bar (RHYTHM vendor HAL)",
        }
        pj.write_text(json.dumps(data, indent=2) + "\n")
        print(f"[2/3] platform.json: 'bar' registriert")
    else:
        print("[2/3] platform.json: 'bar' bereits vorhanden")

    # 3) Board patchen
    board = PLATFORM / "boards/nucleo_f767zi.json"
    if not board.exists():
        print(f"[3/3] Board nicht gefunden: {board}")
        return 1
    bdata = json.loads(board.read_text())
    bfws = bdata.setdefault("frameworks", [])
    if "bar" not in bfws:
        bfws.append("bar")
        board.write_text(json.dumps(bdata, indent=2) + "\n")
        print(f"[3/3] Board gepatcht: frameworks={bfws}")
    else:
        print("[3/3] Board: 'bar' bereits vorhanden")

    print("\nFertig. Build: cd firmware && python -m platformio run")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
