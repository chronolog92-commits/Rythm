---
name: saleae-logic-access
description: Use this skill whenever connecting to, querying, or controlling the Saleae Logic 2 analyzer from this machine via the Python automation API. Covers the working client setup, connection, device enumeration, and the self-improvement protocol. Always load this skill before writing new Logic automation code or debugging why a Logic connection fails.
---

# Saleae Logic 2 Zugang (Projekt-Skill)

Konkreter, maschinenlokaler Zugang zum Saleae Logic 2 Analyzer auf dieser Maschine.
Ergänzt den allgemeinen Skill `saleae-logic-2-usb` (USB-Debugging-Workflow).

## Wann verwenden

- Vor jeder neuen Automation gegen den Logic Analyzer.
- Wenn eine Verbindung zu Port 10430 fehlschlägt oder seltsam reagiert.
- Wenn ein Capture geladen, aufgenommen oder exportiert werden soll.

## Verbindung (bewährt, 2026-09-23)

- Automation-Server: `127.0.0.1:10430`, gehört zu `Logic.exe` (`C:\Program Files\Logic\Logic.exe`).
- Logic 2 Software muss laufen und Automation aktiviert sein (Settings → Automation).
- Protokoll ist gRPC. Der offizielle Python-Client heißt `logic2-automation` (Repo: `github.com/saleae/logic2-automation`, Ordner `python/`).

```python
from saleae import automation

with automation.Manager.connect(address="127.0.0.1", port=10430) as manager:
    for d in manager.get_devices():
        print(d)   # DeviceDesc(device_id=..., device_type=LOGIC, ...)
```

Geprüfter Zustand: Device `A7D1BB81883C0092` (Logic, kein Simulation).

## Installation des Clients (wenn `from saleae import automation` fehlschlägt)

Wichtig: Das PyPI-Paket `saleae` (v0.12.0) ist das ALTE SDK und enthält die
Automation-API NICHT. Es gibt dafür keine neuere PyPI-Version. Richtig ist:

1. `git clone --depth 1 https://github.com/saleae/logic2-automation.git %TEMP%\logic2-automation`
2. Im Ordner `%TEMP%\logic2-automation\python` installieren: `pip install .`
   - Das erzeugt das Namensraum-Paket `saleae.automation` (koexistiert mit altem `saleae`-Paket).
3. gRPC-Protobufs generieren (Wheel enthält `saleae/grpc` NICHT):
   ```
   pip install "grpcio-tools>=1.49.0,<1.50.0"
   python -m grpc_tools.protoc -I %TEMP%\logic2-automation\proto ^
     --python_out=<site-packages>\ --grpc_python_out=<site-packages>\ ^
     %TEMP%\logic2-automation\proto\saleae\grpc\saleae.proto
   ```
   - `<site-packages>` = `C:\Users\Philipp\AppData\Roaming\Python\Python310\site-packages` (User-Install, Python 3.10).
   - Protobuf-Version beachten: grpcio-tools 1.49.x erzwingt `protobuf` 4.25.9.
     Das stiegert TensorFlow/TensorBoard-Konflikte – für Saleae unkritisch.
4. Smoke-Test:
   ```
   python -c "from saleae import automation; m = automation.Manager.connect(address='127.0.0.1', port=10430); print(m.get_devices()); m.close()"
   ```

## Fehlersuche

| Symptom | Ursache / Fix |
|---|---|
| `ImportError: cannot import name 'automation' from 'saleae'` | Nur altes PyPI-Paket installiert → Schritte 1–3 oben. |
| `No module named 'saleae.grpc'` | Protobufs nicht generiert → Schritt 3. |
| "generated using an incompatible version of protobuf" | grpcio/protobuf-Versionsmismatch → `pip install --force-reinstall logic2-automation` + Protobufs neu generieren. |
| `ConnectionRefusedError` 10430 | Logic 2 nicht gestartet oder Automation deaktiviert → App starten, Automation aktivieren. NICHT eigenmächtig Firewall ändern. |
| Port offen, RPC antwortet aber nicht | Logik läuft, Automation evtl. gerade (neu) gestartet; kurz warten und erneut verbinden. |

API-Hinweise (v1.0.11):
- Manager: `get_devices()` (nicht `list_devices`), `load_capture(path)`.
- Capture: `add_analyzer(...)`, `export_data_table(filepath, analyzers=[...])`.
- USB: Analyzer `"USB LS and FS"` mit Settings `{"D+": <ch>, "D-: <ch>}` – Channel-Zuordnung immer vor der Auswertung verifizieren (siehe Skill `saleae-logic-2-usb`).

## Self-Improvement-Pflicht (WICHTIG)

Dieser Skill muss sich selbst weiterentwickeln. Sobald in einer Session
etwas NEUES über den Logic Analyzer oder seine Umgebung gelernt wird,
MUSST du diesen Skill noch in derselben Session aktualisieren:

Auslöser (Beispiele):
- Neue/andere Device-ID oder Hardware (z. B. Logic 8 statt Logic 2).
- Logik-Software-Update (neuer Port, neue API-Methode, neues Protokoll).
- Python-Umgebung ändert sich (anderer Python, andere site-packages).
- Neue funktionierende API-Nutzung (z. B. Trigger, analoge Aufnahme,
  Messwerte, Analyzer-Settings) die über die Dokumentation hinausgeht.
- Neuer Fehler mit neuem Fix.
- Neue Verkabelungsfakten (welches Kabel an welchem Channel, Masse).

Vorgehen bei einer Aktualisierung:
1. Nur Fakten aufnehmen, die in dieser Session verifiziert wurden.
2. Konkreten Datum + kurze Beschreibung der Änderung ergänzen.
3. Aus alter Praxis gewordene, überholte Einträge ersetzen oder als
   überholt markieren – nichts stillschweigend löschen.
4. Am Ende der Session dem Nutzer in einem Satz mitteilen, dass der
   Skill aktualisiert wurde und was geändert wurde.

Änderungshistorie:
- 2026-09-23: Skill angelegt. Client `logic2-automation` 1.0.11 + lokale
  Protobuf-Generierung etabliert. Device `A7D1BB81883C0092` verifiziert.
