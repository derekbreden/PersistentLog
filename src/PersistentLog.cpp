#include "PersistentLog.h"
#include <stdarg.h>

// RP2040 Earle Philhower core uses string modes, not ESP-IDF defines
#ifndef FILE_READ
#define FILE_READ "r"
#endif
#ifndef FILE_WRITE
#define FILE_WRITE "w"
#endif
#ifndef FILE_APPEND
#define FILE_APPEND "a"
#endif

PersistentLog::PersistentLog(fs::FS &fs, const char *path, uint32_t maxBytes)
    : _fs(fs), _path(path), _maxBytes(maxBytes), _currentSize(0), _begun(false), _tsCb(nullptr) {
#ifdef ESP32
  _mutex = xSemaphoreCreateMutex();
#endif
}

bool PersistentLog::begin() {
  _begun = false;

  if (!_ensureParentDir()) {
    return false;
  }

  // Check if file exists and get its size
  if (_fs.exists(_path)) {
    File f = _fs.open(_path, FILE_READ);
    if (!f) return false;
    _currentSize = f.size();
    f.close();
  } else {
    // Create the file
    File f = _fs.open(_path, FILE_WRITE);
    if (!f) return false;
    f.close();
    _currentSize = 0;
  }

  _begun = true;
  _trimIfNeeded();
  return true;
}

void PersistentLog::println(const char *fmt, ...) {
  if (!_begun) return;

  char buf[PERSISTENT_LOG_LINE_BUF];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  String line = _timestamp() + " " + buf + "\n";

#ifdef ESP32
  _lock();
#endif
  _appendRaw(line.c_str(), line.length());
  _trimIfNeeded();
#ifdef ESP32
  _unlock();
#endif
}

void PersistentLog::print(const char *fmt, ...) {
  if (!_begun) return;

  char buf[PERSISTENT_LOG_LINE_BUF];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

#ifdef ESP32
  _lock();
#endif
  _appendRaw(buf, strlen(buf));
  _trimIfNeeded();
#ifdef ESP32
  _unlock();
#endif
}

void PersistentLog::write(const char *text) {
  if (!_begun) return;

#ifdef ESP32
  _lock();
#endif
  _appendRaw(text, strlen(text));
  _trimIfNeeded();
#ifdef ESP32
  _unlock();
#endif
}

void PersistentLog::dump(Stream &out) {
  if (!_begun) return;

#ifdef ESP32
  _lock();
#endif

  File f = _fs.open(_path, FILE_READ);
  if (!f) {
#ifdef ESP32
    _unlock();
#endif
    return;
  }

  uint8_t buf[256];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    out.write(buf, n);
  }
  f.close();

#ifdef ESP32
  _unlock();
#endif
}

void PersistentLog::dumpLast(Stream &out, int n) {
  if (!_begun || n <= 0) return;

#ifdef ESP32
  _lock();
#endif

  File f = _fs.open(_path, FILE_READ);
  if (!f) {
#ifdef ESP32
    _unlock();
#endif
    return;
  }

  // Read the whole file to find line boundaries.
  // For typical embedded log sizes (16-64KB) this is fine.
  uint32_t fsize = f.size();
  if (fsize == 0) {
    f.close();
#ifdef ESP32
    _unlock();
#endif
    return;
  }

  // Scan backwards for newlines. We read in chunks from the end.
  // Strategy: find byte offset of the Nth-from-last newline.
  int newlineCount = 0;
  uint32_t offset = fsize;
  uint8_t buf[256];

  while (offset > 0 && newlineCount <= n) {
    uint32_t chunkSize = (offset < sizeof(buf)) ? offset : sizeof(buf);
    offset -= chunkSize;
    f.seek(offset);
    f.read(buf, chunkSize);

    for (int i = (int)chunkSize - 1; i >= 0; i--) {
      if (buf[i] == '\n') {
        newlineCount++;
        if (newlineCount > n) {
          // This newline is just before the Nth-from-last line
          offset += i + 1;
          goto found;
        }
      }
    }
  }

found:
  // Now dump from offset to end
  f.seek(offset);
  while (f.available()) {
    size_t bytesRead = f.read(buf, sizeof(buf));
    out.write(buf, bytesRead);
  }
  f.close();

#ifdef ESP32
  _unlock();
#endif
}

void PersistentLog::clear() {
  if (!_begun) return;

#ifdef ESP32
  _lock();
#endif

  // Overwrite with empty file
  File f = _fs.open(_path, FILE_WRITE);
  if (f) f.close();
  _currentSize = 0;

#ifdef ESP32
  _unlock();
#endif
}

uint32_t PersistentLog::size() {
  return _currentSize;
}

uint32_t PersistentLog::capacity() {
  return _maxBytes;
}

uint32_t PersistentLog::lineCount() {
  if (!_begun) return 0;

#ifdef ESP32
  _lock();
#endif

  File f = _fs.open(_path, FILE_READ);
  if (!f) {
#ifdef ESP32
    _unlock();
#endif
    return 0;
  }

  uint32_t count = 0;
  uint8_t buf[256];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    for (size_t i = 0; i < n; i++) {
      if (buf[i] == '\n') count++;
    }
  }
  f.close();

#ifdef ESP32
  _unlock();
#endif
  return count;
}

void PersistentLog::setTimestampCallback(TimestampCallback cb) {
  _tsCb = cb;
}

// --- Private ---

void PersistentLog::_appendRaw(const char *text, size_t len) {
  File f = _fs.open(_path, FILE_APPEND);
  if (!f) return;
  f.write((const uint8_t *)text, len);
  f.close();
  _currentSize += len;
}

void PersistentLog::_trimIfNeeded() {
  if (_currentSize <= _maxBytes) return;

  // Read the file, keep the most recent TRIM_RATIO portion
  File f = _fs.open(_path, FILE_READ);
  if (!f) return;

  uint32_t fsize = f.size();
  uint32_t targetSize = (uint32_t)(_maxBytes * PERSISTENT_LOG_TRIM_RATIO);
  uint32_t skipBytes = fsize - targetSize;

  // Seek past the skip region, then find the next newline to avoid partial lines
  f.seek(skipBytes);
  while (f.available()) {
    int c = f.read();
    skipBytes++;
    if (c == '\n') break;
  }

  // Read the remainder into a temp file
  String tmpPath = String(_path) + ".tmp";
  File tmp = _fs.open(tmpPath.c_str(), FILE_WRITE);
  if (!tmp) {
    f.close();
    return;
  }

  uint8_t buf[256];
  uint32_t written = 0;
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    tmp.write(buf, n);
    written += n;
  }
  f.close();
  tmp.close();

  // Replace original with trimmed version
  _fs.remove(_path);
  _fs.rename(tmpPath.c_str(), _path);
  _currentSize = written;
}

String PersistentLog::_timestamp() {
  if (_tsCb) {
    return "[" + _tsCb() + "]";
  }
  // Default: millis since boot, zero-padded to 10 digits
  char ts[16];
  snprintf(ts, sizeof(ts), "[%010lu]", (unsigned long)millis());
  return String(ts);
}

bool PersistentLog::_ensureParentDir() {
  String pathStr(_path);
  int lastSlash = pathStr.lastIndexOf('/');
  if (lastSlash <= 0) return true; // root level, no dir needed

  String dir = pathStr.substring(0, lastSlash);
  if (_fs.exists(dir.c_str())) return true;

  return _fs.mkdir(dir.c_str());
}

#ifdef ESP32
void PersistentLog::_lock() {
  if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
}

void PersistentLog::_unlock() {
  if (_mutex) xSemaphoreGive(_mutex);
}
#endif
