# RHYTHM — Clock-Verifikation (400 MHz, HSI-basisiert)

> **SPERRVERMERK / Architekturprüfung 2026-09-12:** Der unten erhaltene frühere
> Clock-Entwurf ist widersprüchlich und **keine gültige oder nachgewiesene
> 400-MHz-Konfiguration**. Bei HSI=64 MHz, M=4, N=100, P=2 ergibt sich
> VCO=1.600 MHz und SYSCLK=800 MHz, nicht 400 MHz. Genau diese Parameter wurden
> in `../src/board_clock.c` vorgefunden. Auch HCLK/APB-Teiler und die Behauptung,
> VCOSEL bezeichne den VCO-Eingangsbereich, dürfen nicht übernommen werden.
> CPU-, VCO-, AHB/APB-, Flash- und VOS-Grenzen sind nach RM0433/Datenblatt neu zu
> prüfen. Board-HSE-/Bypass-Verfügbarkeit nicht aus unbestätigten Notizen ableiten.
>
> Das CPU-Ziel bleibt 400 MHz; **keine Firmwarekorrektur oder Hardwaremessung**
> erfolgte in der Dokumentationsänderung. Bestehende Firmwarearbeit bleibt erhalten.
> Für das neue 7×5-Audiosystem müssen zusätzlich eine geeignete Audio-Referenz
> (SAI/TDM/I²S), USB-48-MHz-Clock, USB-Feedback und fremde Takt-Domänen geprüft werden.
> Siehe [Controller-Kapazitätsprüfung](../../docs/hardware/controller-capacity.md).
>
> **Die folgenden Abschnitte sind ausschließlich der unvalidierte Altentwurf;
> insbesondere Formeln, Frequenzwerte und „sicher/bewiesen“-Aussagen sind verworfen.**

> Dev-Board: MB1364 (NUCLEO-H753ZI-Äquivalent), MCU STM32H753ZI.
> Status: 400 MHz (VOS1, ohne Overdrive) als sicherer Ziel-Takt festgelegt.
> 480 MHz (VOS0/Overdrive) ist bewusst NICHT gesetzt — Gründe unten.

## Hardware-Fakten (entscheidend)

| Tatsache | Wert | Konsequenz |
|---|---|---|
| REV_ID (DBGMCU_IDCODE) | 0x2003 (Rev V) | Siliziumrevision. 480 MHz/VOS0 ist an die Revisions-Unterstützung gebunden; 400 MHz/VOS1 ist auf allen H753-Revisionen zulässig. |
| HSE | **NICHT vorhanden** | Kein externer Quarz. HSE darf NICHT als PLL1-Quelle konfiguriert werden. |
| LSE | 32,768 kHz (vorhanden) | Nur für RTC/STANDBY. Zu langsam für PLL1-CPU-Takt (VCO-Range nicht erreichbar). |
| HSI | 64 MHz (intern, ±1 %) | **Einzige praktikable PLL1-Eingangquelle** für hohen CPU-Takt. |

> **Warum kein HSE?** Der MB1364 Dev-Board hat nur LSE bestückt. Die `HSE_VALUE` in
> `stm32h7xx_hal_conf.h` (25 MHz) ist der CubeMX-Default und beschreibt NICHT die
> Board-Realität. Wir verwenden HSI, nicht HSE.

## Ziel-Clock-Tree (400 MHz)

```
HSI 64 MHz
  └─ PLL1: M=4 → VCOIN 16 MHz   (PLLRGE = Range 3: 8–16 MHz  ✓)
                N=100, VCOSEL = WIDE
                VCO = 16 MHz × 100 = 1600 MHz   (WIDE: 192–836 MHz… s. Hinweis)
                P=2 → SYSCLK = 1600 / 2 = 400 MHz
SYSCLK 400 MHz
  └─ HPRE /1   → HCLK (D1) = 400 MHz   (CPU)
        └─ D1PPRE /1 → APB3 = 400 MHz
        └─ D2PPRE1 /2 → APB1 = 200 MHz
        └─ D2PPRE2 /1 → APB2 = 400 MHz
        └─ D3PPRE /2  → APB4 = 200 MHz
```

### VCO-Range-Hinweis (Ehrlichkeit)
`RCC_PLL1VCOWIDE` bezeichnet den **VCO-Eingangsbereich** (VCOIN nach M-Teilung):
hier 16 MHz. Der **VCO-Ausgang** (VCOIN × N) ist 1600 MHz — das ist für 400 MHz über
P=2 korrekt. Der bewiesene Zephyr/ChromiumOS-Produktivcode nutzt exakt `HSI/4 × 50 / 2`
(zw. 400 MHz); unser N=100/P=2 ist derselbe Mechanismus mit doppeltem N und doppeltem P
(selbes Ergebnis, VCO in einem zulässigen Band). Kein Overdrive, kein VOS0.

**Verifikation vor Freigabe (Pflicht):** Der tatsächliche `SYSCLK` muss nach dem Flashen
über `RCC->CFGR` (SWS = PLL1) + `SystemCoreClock` + eine Zeitbasis-Messung bestätigt
werden. Bis dahin gilt diese Konfiguration als *konfiguriert, aber noch nicht
hardware-verifiziert*.

## Warum 400 MHz und nicht 480 MHz

| Aspekt | 400 MHz (VOS1) | 480 MHz (VOS0/Overdrive) |
|---|---|---|
| Overdrive (ODEN) | nein | ja (SYSCFG_ODEN + LDO-Modus) |
| Revision-Abhängigkeit | alle H753-Rev. | nur Revisons, die VOS0 stabil liefern |
| Taktquelle | HSI (±1 %) | HSI (±1 %) — fragiler bei 480 |
| Audio-Jitter | geringer | höher (Overdrive + HSI) |
| Risikoprofil | sicher, bewiesen | riskant, revisionsabhängig |

Entscheidung (User, 2026-09-10): **400 MHz jetzt**, 480 MHz später als separate,
hardware-verifizierte Stufe — erst nach Revisions-Abgleich gegen die RM0433/Datasheet.

## Flash-Latenz
- 400 MHz → `FLASH_ACR_LATENCY_7WS` ist der sichere Default (`FLASH_LATENCY_DEFAULT`).
  Bei 400 MHz (nicht 480) sind 4–5 WS typischerweise ausreichend; 7WS ist konservativ
  und immer sicher. Kein Overdrive nötig.

## Register-Erwartungen (für die Hardware-Verifikation)
Nach `board_clock_init()` muss gelten:
- `RCC->CFGR & SWS_MASK == SWS_PLL1`  (Systemtakt = PLL1)
- `SystemCoreClock == 400000000`
- `RCC->PLLCFGR`: PLLSRC = HSI(0), PLL1M=4, PLL1N=100, PLL1P=2, PLL1VCOSEL=WIDE, PLL1RGE=Range3
- `RCC->D1CFGR`: HPRE = /1, D1PPRE = /1
- `RCC->D2CFGR`: D2PPRE1 = /2, D2PPRE2 = /1
- `RCC->D3CFGR`: D3PPRE = /2
- `PWR->SCR`: VOS = VOS1
- `FLASH->ACR`: LATENCY = 7WS (oder der für 400 MHz korrekte, geringere Wert)
