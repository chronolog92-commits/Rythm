# RHYTHM USB Debugging - Schritt-für-Schritt-Anleitung

## Symptom
Das Gerät erscheint **nicht** im Windows Device Manager. Keine Fehler über UART.

## Mögliche Ursachen (in Reihenfolge der Wahrscheinlichkeit)

### 1. Hardware-Probleme
- **USB-Kabel:** Ist es ein Datenkabel (nicht nur Ladekabel)? 
- **VBUS:** Liegt 5 V an PA9? (Multimeter messen)
- **PHY-Anschluss:** Sind PA11/PA12 korrekt mit DM/DP verbunden?
- **Stromversorgung:** Hat das Board genug Strom?

### 2. Firmware-Probleme
- **OTG_HS-Clock:** Ist `RCC->AHB1ENR.USB1OTGHSEN` gesetzt?
- **IRQ-Handler:** Ist `OTG_HS_IRQHandler` aktiv?
- **PHY-Setup:** Sind PA11/PA12 als AF10 konfiguriert?
- **USB-48-MHz:** Ist HSI48 korrekt konfiguriert?

### 3. Windows-Probleme
- **Treiber:** Fehlender USB-Audio-Treiber?
- **Root-Hub:** USB-Port aktiv? (Andere Geräte testen)
- **Gerätemanager:** Erscheint als "Unknown Device"?

## Debug-Schritte

### Schritt 1: Serielle Konsole überwachen
```bash
# Windows: COM-Port identifizieren (z.B. COM3)
# Putty / Tera Term / minicom @ 115200 baud

# Erwartete Ausgabe nach Flashen:
RHYTHM firmware started (UAC2, 48000 Hz, 2ch, 24-bit)
USB Audio Device initialized (TinyUSB UAC2, OTG_HS Rhport1, FS mode)
Waiting for USB enumeration...
[USB] mounted (enumeration OK)  <-- erscheint NACH dem USB-Anschluss
```

**Wenn "Waiting for USB enumeration..." erscheint, aber nicht "mounted":**
- USB-Controller läuft, aber keine Enumeration
- Mögliche Ursachen: VBUS fehlt, PHY-Problem, IRQ-Problem

**Wenn "ERROR: tusb_init() FAILED" erscheint:**
- OTG_HS-Clock nicht aktiv oder IRQ-Handler fehlt

**Wenn keine Ausgabe erscheint:**
- UART-Problem (falscher COM-Port, falsche Baudrate, UART-GPIO nicht init)

### Schritt 2: Windows Device Manager prüfen
```
Win + X -> Gerätemanager
Oder: devmgmt.msc
```

**Mögliche Szenarien:**
1. **Kein neues Gerät:** USB-Controller nicht aktiv (Firmware-Problem)
2. **"Unknown Device" (unbekanntes Gerät):** Gerät enumeriert, aber kein Treiber
3. **"Audio Device" mit gelbem Ausrufezeichen:** Treiber-Problem
4. **"USB Composite Device":** ERFOLG - Gerät ist erkannt!

### Schritt 3: USB-Pins messen (mit Oszilloskop oder Multimeter)
```
PA12 (DP): Sollte nach dem USB-Anschluss einen 1.5k-Pull-up zeigen
            (3.3 V über den internen Pull-up des OTG-Controllers)
PA11 (DM): Sollte auf GND sein (kein Signal ohne Host)
PA9 (VBUS): Sollte 5 V zeigen, wenn USB-Kabel angeschlossen ist
```

### Schritt 4: Firmware-Debug-Output analysieren

### A) Register-Dump (nach `tusb_init()`)
Die Firmware zeigt jetzt einen **Register-Dump** des OTG_HS-Controllers:

```
=== USB Controller Register Dump ===
GSNPSID:   0x4F544737 (erwartet: 0x4F544737)
GHWCFG2:   0x00080000 (hs_phy=0, fs_phy=1)
DCFG:      0x00000000
GINTSTS:   0x00000000
AHB1ENR:   0x00000000 (USB1OTGHSEN=bit28)
=====================================
```

**Interpretation:**
- **GSNPSID = 0x4F544737** → OTG-Controller ist aktiv (Clock OK)
- **GSNPSID = 0x00000000** → OTG_HS-Clock fehlt! (Problem)
- **fs_phy=1** → Interne FS-PHY erkannt (PA11/PA12 ok)
- **fs_phy=0** → Keine FS-PHY! (Pin-Verdrahtung prüfen)
- **AHB1ENR bit28 = 1** → OTG_HS-Clock aktiv
- **AHB1ENR bit28 = 0** → OTG_HS-Clock fehlt!

### B) Warnung nach 5 s ohne Enumeration
```
WARN: USB not mounted after 5 s. Check:
  1. USB cable connected (VBUS on PA9?)
  2. OTG_HS clock enabled (RCC->AHB1ENR.USB1OTGHSEN)
  3. IRQ handler active (OTG_HS_IRQHandler)
  4. PHY type: FS internal (PA11/12, AF10)
```

**Dieser Warnhinweis hilft, das Problem einzugrenzen.**

## Nächste Schritte (je nach Debug-Ergebnis)

### Wenn VBUS fehlt (PA9 = 0 V):
- USB-Kabel prüfen (Datenkabel vs. Ladekabel)
- USB-Port am PC testen (anderer Port)
- Board-Stromversorgung prüfen

### Wenn OTG_HS-Clock nicht aktiv:
- `bsp_usb_fs_init()` prüfen: `__HAL_RCC_USB1_OTG_HS_CLK_ENABLE()` vorhanden?
- Alternativ: Clock direkt im Code setzen und GSNPSID-Register lesen

### Wenn IRQ-Handler nicht aktiv:
- `OTG_HS_IRQHandler` in der Firmware vorhanden?
- `dcd_int_handler(1)` aufgerufen?
- Alternativ: Manuellen IRQ-Test (gintsts-Register lesen)

### Wenn PHY-Setup fehlerhaft:
- PA11/PA12 als AF10 konfiguriert?
- Pull-up an DP (wird vom OTG-Controller gestellt)?
- Alternativ: USB2_OTG_FS (PA2/PA3) testen

### Wenn Windows "Unknown Device" zeigt:
- Treiber installieren (Windows hat UAC2-Treiber)
- Gerätemanager: "Treiber aktualisieren"
- Alternativ: Linux testen (usbmon, lsusb)

## Quick-Check: USB-Controller-Register lesen
Falls die Firmware nicht enumeriert, können wir die Register direkt lesen:
```c
// In main.c nach tusb_init() einfügen:
#include "stm32h7xx.h"

// GSNPSID lesen (sollte 0x4F544737 sein für STM32)
uint32_t gsnpsid = *(volatile uint32_t*)0x40040004;
printf("GSNPSID: 0x%08X\r\n", gsnpsid);

// GHWCFG2 lesen (PHY-Typ)
uint32_t ghwcfg2 = *(volatile uint32_t*)0x40040014;
printf("GHWCFG2: 0x%08X (hs_phy_type=%d, fs_phy_type=%d)\r\n", 
       ghwcfg2, (ghwcfg2 >> 18) & 0x3, (ghwcfg2 >> 20) & 0x3);

// DCFG lesen (Device Config)
uint32_t dcfg = *(volatile uint32_t*)0x40040400;
printf("DCFG: 0x%08X\r\n", dcfg);
```

**Erwartete Werte:**
- GSNPSID = 0x4F544737 ("OTG7" = OTG v3.30a)
- GHWCFG2: hs_phy_type=0 (kein HS), fs_phy_type=1 (FS vorhanden)
- DCFG: zeigt, ob der Controller im FS-Modus ist
