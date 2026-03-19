# PersistentLog

A ring buffer logger for Arduino that writes to LittleFS (or SPIFFS). Fixed storage budget. Oldest entries drop off automatically. Survives power cycles. Retrievable over serial or programmatically.

Your device has memory and a filesystem. Use them.

## Why

You're about to add `Serial.println()` for debugging. Do this instead — same effort, but now you can see what happened *last time* the device crashed.

## Install

**PlatformIO:**
```ini
lib_deps = https://github.com/derekbreden/PersistentLog.git
```

**Arduino IDE:**
Download ZIP from GitHub, then Sketch > Include Library > Add .ZIP Library.

## Quick Start

```cpp
#include <LittleFS.h>
#include <PersistentLog.h>

PersistentLog logger(LittleFS, "/logs/system.log", 32768);  // 32KB budget

void setup() {
  Serial.begin(115200);
  LittleFS.begin(true);
  logger.begin();
  logger.println("Boot complete — free heap: %lu", (unsigned long)ESP.getFreeHeap());
}

void loop() {
  if (somethingWentWrong) {
    logger.println("BLE upload CRC mismatch: got 0x%08X expected 0x%08X", got, expected);
  }

  // Dump over serial anytime:
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    if (cmd == "DUMP") logger.dump(Serial);
  }
}
```

## API Reference

```cpp
PersistentLog(fs::FS &fs, const char *path, uint32_t maxBytes = 32768);
```
Constructor. Pass your filesystem (`LittleFS`, `SPIFFS`), a file path, and an optional storage budget in bytes.

```cpp
bool begin();
```
Open or create the log file. Call after `LittleFS.begin()`. Returns `false` if the file can't be opened. Trims to budget if the existing log is oversized.

```cpp
void println(const char *fmt, ...);
```
Printf-style write with automatic timestamp prefix and newline. This is the main method you'll use.

```cpp
void print(const char *fmt, ...);
```
Printf-style write, no newline, no timestamp.

```cpp
void write(const char *text);
```
Raw write. No timestamp, no formatting.

```cpp
void dump(Stream &out);
```
Write the entire log to a stream. `logger.dump(Serial)` prints it over USB.

```cpp
void dumpLast(Stream &out, int n);
```
Write the last N lines to a stream.

```cpp
void clear();
```
Erase the log.

```cpp
uint32_t size();       // Current log size in bytes
uint32_t capacity();   // Max budget in bytes
uint32_t lineCount();  // Approximate line count
```

```cpp
void setTimestampCallback(TimestampCallback cb);
```
Override the default millis() timestamp with your own. Useful for RTC wall-clock time:
```cpp
logger.setTimestampCallback([]() -> String {
  DateTime now = rtc.now();
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buf);
});
```

## Timestamps

Default format uses millis since boot, zero-padded:

```
[0000000342] Boot complete — free heap: 253412
[0000015203] BLE upload CRC mismatch: got 0x3F1046FD expected 0x90D58100
[0000015204] Sensor timeout on I2C bus
```

With an RTC callback:

```
[2026-03-19 14:23:01] Boot complete — free heap: 253412
```

## How It Works

Single file, append-only. When the file exceeds your budget, the library rewrites it keeping the most recent 75% of content (trimming at a newline boundary so no partial lines). LittleFS handles wear leveling at the filesystem level.

This is simple on purpose. For 16-64KB logs with infrequent writes, rewriting once in a while is fine.

## Flash Wear

LittleFS does wear leveling internally. The library only rewrites the file when the budget is exceeded — not on every write.

For a 32KB log on a 4MB flash chip with 100,000 write-cycle endurance per sector:

- Each `println` appends ~80 bytes (typical line)
- Budget exceeded every ~400 writes, triggering one rewrite
- At 1 write per second (aggressive): ~400 rewrites per day
- Each rewrite touches ~24KB across multiple 4KB sectors
- With wear leveling across ~1000 sectors: **years of life**

At more typical logging rates (a few writes per minute), flash wear is negligible.

If you're logging at high frequency and worried about wear, increase the budget or decrease the trim ratio:

```cpp
#define PERSISTENT_LOG_TRIM_RATIO 0.5f  // keep 50% instead of 75%
```

## Thread Safety

On ESP32 (dual-core), all public methods are guarded by a FreeRTOS mutex. Safe to call `println()` from one task and `dump()` from another. On single-core platforms, no mutex overhead.

## Platform Support

Tested on:
- ESP32
- ESP32-S3

Should work on any Arduino-compatible board with LittleFS or SPIFFS:
- ESP32, ESP32-S3, ESP32-C3
- ESP8266
- RP2040

Pass `SPIFFS` instead of `LittleFS` to use SPIFFS:
```cpp
PersistentLog logger(SPIFFS, "/log.txt", 16384);
```

## Compile-Time Configuration

```cpp
#define PERSISTENT_LOG_DEFAULT_MAX_BYTES 32768  // Default budget
#define PERSISTENT_LOG_LINE_BUF 256             // Max single line length
#define PERSISTENT_LOG_TRIM_RATIO 0.75f         // Keep this fraction on trim
```

Define these before `#include <PersistentLog.h>` or in your build flags.

## License

MIT
