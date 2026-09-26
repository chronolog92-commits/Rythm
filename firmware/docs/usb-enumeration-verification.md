# USB-Audio-Enumeration: Hardware-Verifikation (2026-09-26)

Board: NUCLEO-H753ZI, TinyUSB UAC1, OTG_FS / Rhport 0.
Debug: ST-Link V3 / OpenOCD 0.12, Windows USB-Audiotreiber `usbaudio`.

## Nachgewiesene Fehler und Korrekturen

1. **USB-Versorgungsfreigabe fehlte.** `PWR->CR3 = 0x42`, somit
   `USB33DEN=0` und `USB33RDY=0`. Allein das Setzen von USB33DEN ergab
   `0x05000042`, laufende SOF-Frames und eine USB-Adresse.
   `bsp_usb_fs_init()` aktiviert jetzt den USB-Spannungsdetektor vor TinyUSB.
2. **HardFault beim Seriennummern-Deskriptor.** Gemessen wurden
   `CFSR=0x8200`, `HFSR=0x40000000`, `BFAR=0x1FFF7590`.
   Der Callback verwendete eine UID-Adresse einer anderen STM32-Familie.
   Er verwendet jetzt `UID_BASE` (H753: `0x1FF1E800`) und kodiert die
   vollständige 96-Bit-UID unter Beachtung der Pufferlänge.
3. **Windows-Audiotreiber startete nicht (Code 10).** Nach dem UID-Fix war
   das Gerät konfiguriert, aber der letzte SETUP-Request war
   `A1 81 00 01 00 02 01 00`: UAC1 GET_CUR, Feature Unit 2, Master Mute,
   Länge 1. Beide EP0-Richtungen waren gestallt. Der Deskriptor bot
   Mute/Volume an, die Anwendung hatte jedoch keine Control-Callbacks.
   Ergänzt sind Mute GET/SET_CUR, Volume GET/SET_CUR und GET_MIN/MAX/RES
   für Master/links/rechts sowie Endpoint GET/SET_CUR für 44,1/48 kHz.
4. **Flash-Verifikation schlug am RAM-Puffer fehl.** `.ram_audio_buf`
   wurde als ladbarer ELF-Abschnitt bei `0x24000000` ausgegeben.
   Der uninitialisierte Ringpuffer ist jetzt `NOLOAD`. Seine Verwaltungsdaten
   werden weiterhin von `audio_buffer_init()` initialisiert.

Zusätzlich korrigiert: HSI48 entspricht USBSEL=3; ACTVOSRDY ist Bit 13.
Die Diagnose zeigt jetzt auch USB33DEN und USB33RDY.

## Reproduzierbare Prüfung

Nach Linkerskriptänderungen einen vollständigen Neubau verwenden:

```powershell
pio run -e nucleo-h753zi -t clean
pio run -e nucleo-h753zi
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c 'gdb port disabled; tcl port disabled; telnet port disabled; program .pio/build/nucleo-h753zi/firmware.elf verify reset exit'
```

Erwartet: Build SUCCESS und OpenOCD `Verified OK`.

```powershell
Get-PnpDevice -PresentOnly | Where-Object {
    $_.InstanceId -match 'VID_CAFE' -or $_.FriendlyName -match 'UAC1 Speaker'
} | Format-Table Status, Class, FriendlyName, InstanceId -AutoSize
```

Gemessen nach Flash und Reset:

| Status | Klasse | Gerät |
|---|---|---|
| OK | USB | USB-Verbundgerät, VID_CAFE/PID_401B |
| OK | MEDIA | UAC1 Speaker |
| OK | AudioEndpoint | Lautsprecher (UAC1 Speaker) |

## Umfang der Verifikation

- Die Fehler wurden zunächst auf Hardware beobachtet und danach mit den
  jeweiligen Korrekturen erneut geprüft.
- Die Enumeration funktioniert mit unveränderter AF12-Einstellung.
  GPIO-IDR zeigte dabei weiter Low für PA11/PA12; diese Lesung ist hier
  kein zuverlässiger Nachweis des elektrischen USB-Leitungspegels.
- Audio-Geräteerkennung ist geprüft; kontinuierliches PCM-Streaming und
  hörbare SAI/DAC-Ausgabe sind damit nicht verifiziert. Mute/Volume werden
  derzeit als Control-Zustand gespeichert, nicht auf PCM-Samples angewendet.
- Kein Saleae-Trace aufgenommen: Kanalzuordnung D+/D- war nicht bekannt.
  Die Diagnose beruht auf SWD-Registern, dem gespeicherten SETUP-Request
  des USB-Stacks und dem Windows-Gerätestatus.
