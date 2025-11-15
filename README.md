# ESP Meteostanice

Projekt obsahuje firmware pro meteostanici postavenou na ESP8266 nebo ESP32-C3. Firmware kombinuje měření z čidel HTU21D, BH1750 a BMP180/BMP085, publikuje je přes Modbus TCP a nabízí webové rozhraní s ovládáním.

## Hlavní vlastnosti
- **Podpora ESP8266 i ESP32-C3** včetně automatické detekce pinů I2C a LED.
- **Wi-Fi konfigurace přes WiFiManager** s možností vyvolání portálu krátkým stiskem tlačítka FLASH.
- **Modbus TCP server** (port 502) s mirrorováním hlavních hodnot a možností online úprav konfigurace.
- **Integrovaný webový server** s jednostránkovou aplikací (SPA) pro živý přehled hodnot, konfiguraci a diagnostiku.
- **Automatická kalibrace QNH (AutoQNH)** využívající online zdroje METAR (NOAA TGFTP) s možností manuálního nastavení ICAO.
- **Sériová CLI konzole** pro rychlé ladění a změnu parametrů, včetně spouštění autotestu, skenu I2C a ručního rebootu.

## Přehled adresářů
```
.
├── README.md        – Tento dokument
├── data/            – Obsah LittleFS (SPA UI v `ui/index.html`)
├── HttpApiUi.h      – REST API a obsluha HTTP serveru
└── main.ino         – Hlavní firmware, senzory, Modbus a logika AutoQNH
```

## Kompilace a nahrání firmware
1. Otevřete projekt v Arduino IDE (≥1.8) nebo PlatformIO.
2. Doinstalujte závislosti:
   - `WiFiManager`
   - `ArduinoJson`
   - `BH1750`
   - `SparkFun HTU21D`
   - `Adafruit BMP085`
   - `emelianov/Modbus-ESP8266`
3. V nastavení projektu zvolte příslušnou desku (ESP8266/ESP32-C3) a správný port.
4. Nahrajte firmware do zařízení.
5. Nahrajte obsah adresáře `data/` do LittleFS (Arduino IDE: nástroj *ESP8266 LittleFS Data Upload* / *ESP32 Sketch Data Upload*, PlatformIO: `pio run -t uploadfs`).

> **Tip:** Pro ESP32-C3 doporučuji nastavit `I2C_SDA_PIN` a `I2C_SCL_PIN` na 8/9, případně upravit definice v `main.ino` podle použité desky.

## Inicializace
1. Po nahrání se zařízení spustí v běžném režimu. Krátký stisk tlačítka **FLASH (GPIO0)** aktivuje konfigurační portál WiFiManageru.
2. Připojte se k vytvořené Wi-Fi síti a nastavte:
   - Název zařízení (hostname)
   - Modbus Unit ID
   - Periodu měření
   - Nadmořskou výšku, QNH a mód výpočtu nadmořské výšky
   - Parametry AutoQNH (perioda, manuální ICAO)
3. Po uložení se hodnoty persistují do `LittleFS` (`/config.json`) a promítnou se do Modbus registrů.

## Webové rozhraní
- Web UI je dostupné na `/ui` a zobrazuje se automaticky při otevření kořenové URL.
- Tab „Přehled“ zobrazuje aktuální hodnoty senzorů, odvozené veličiny a stav sítě.
- Tab „Nastavení“ dovoluje měnit konfiguraci. Změny lze uložit do flash (`persist=true`) nebo pouze dočasně aplikovat.
- Tab „Nástroje“ nabízí diagnostické akce (I2C scan, self-test, ruční spuštění AutoQNH, reboot).

> HTML/JS front-end je uložený v LittleFS (`data/ui/index.html`), takže jej můžete upravovat a nahrávat nezávisle na firmware.

### REST API
| Endpoint | Metoda | Popis |
|----------|--------|-------|
| `/api/status` | GET | Živé hodnoty senzorů, stav Wi-Fi, AutoQNH a výsledky posledního I2C skenu. |
| `/api/config` | GET/POST | Čtení a změna konfigurace. POST umožňuje volbu `persist` pro uložení do LittleFS. |
| `/api/action` | POST | Příkazy `scan`, `selftest`, `aq_run`, `reboot`. |
| `/api/mb` | GET/POST | Přímé čtení a zápis Modbus holding registrů. |

## Modbus holding registry (výběr)
- `0` – bitmask stavů senzorů (HTU/BH1750/BMP)
- `10..16` – surové hodnoty teploty, vlhkosti, tlaku a osvitu
- `32..35` – QNH [Pa] a ALT [cm]
- `36..44` – konfigurace nadmořské výšky a AutoQNH
- `45..53` – diagnostika AutoQNH (výsledek, ICAO, poslední update)
- `70..77` – echo self-test Modbus (pro komunikaci s Loxone)
- `90..111` – výsledky I2C skenu
- `100..102` – příkazy (uložení+reboot, I2C scan, self-test)

Detailní popis všech registrů je uveden přímo v hlavičce `main.ino`.

## Sériová CLI
Připojte se přes sériový port (115200 baud) a zadejte `help` pro zobrazení dostupných příkazů. CLI umožňuje měnit parametry, ukládat/načítat konfiguraci, spouštět AutoQNH a diagnostické akce nebo přímo zapisovat do Modbus registrů.

## Optimalizace a audit
- API odpovědi jsou generovány pomocí `ArduinoJson`, čímž se minimalizuje fragmentace haldy a zlepší výkon webového rozhraní.
- Chybové odpovědi mají jednotný formát JSON, což usnadňuje zpracování ve frontendu.
- `api/status` sdílí předpočítané hodnoty Wi-Fi stavu a IP adresy, díky čemuž se eliminuje vícenásobné volání `WiFi.status()`.

## Licence
Zdrojový kód je poskytován „tak jak je“. Používejte dle potřeby, s ohledem na licence jednotlivých knihoven.
