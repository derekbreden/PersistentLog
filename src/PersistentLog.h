#ifndef PERSISTENT_LOG_H
#define PERSISTENT_LOG_H

#include <Arduino.h>
#include <FS.h>

// Default: 32KB log budget
#ifndef PERSISTENT_LOG_DEFAULT_MAX_BYTES
#define PERSISTENT_LOG_DEFAULT_MAX_BYTES 32768
#endif

// Printf buffer size for a single log line
#ifndef PERSISTENT_LOG_LINE_BUF
#define PERSISTENT_LOG_LINE_BUF 256
#endif

// When log exceeds budget, trim to this fraction of maxBytes
#ifndef PERSISTENT_LOG_TRIM_RATIO
#define PERSISTENT_LOG_TRIM_RATIO 0.75f
#endif

class PersistentLog {
public:
  // path: file path on the filesystem (e.g. "/logs/system.log")
  // maxBytes: storage budget — oldest lines trimmed to fit
  // fs: filesystem reference (LittleFS, SPIFFS, etc.)
  PersistentLog(fs::FS &fs, const char *path, uint32_t maxBytes = PERSISTENT_LOG_DEFAULT_MAX_BYTES);

  // Open/create the log file. Call after your filesystem is mounted.
  // Returns false if the file can't be opened.
  bool begin();

  // Printf-style logging with automatic timestamp and newline.
  void println(const char *fmt, ...);

  // Printf-style logging, no newline.
  void print(const char *fmt, ...);

  // Raw write — no timestamp, no formatting.
  void write(const char *text);

  // Dump entire log to a stream (Serial, etc.)
  void dump(Stream &out);

  // Dump the last N lines.
  void dumpLast(Stream &out, int n);

  // Erase the log file.
  void clear();

  // Current log size in bytes.
  uint32_t size();

  // Max budget in bytes.
  uint32_t capacity();

  // Approximate line count.
  uint32_t lineCount();

  // Set a callback that returns a timestamp string.
  // If not set, uses millis().
  // Example: logger.setTimestampCallback([]() -> String { return rtc.now().timestamp(); });
  typedef String (*TimestampCallback)();
  void setTimestampCallback(TimestampCallback cb);

private:
  fs::FS &_fs;
  const char *_path;
  uint32_t _maxBytes;
  uint32_t _currentSize;
  bool _begun;
  TimestampCallback _tsCb;

  void _appendRaw(const char *text, size_t len);
  void _trimIfNeeded();
  String _timestamp();
  bool _ensureParentDir();

#ifdef ESP32
  SemaphoreHandle_t _mutex;
  void _lock();
  void _unlock();
#endif
};

#endif // PERSISTENT_LOG_H
