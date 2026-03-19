#include <LittleFS.h>
#include <PersistentLog.h>

PersistentLog logger(LittleFS, "/logs/system.log", 32768);  // 32KB budget

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
    return;
  }

  if (!logger.begin()) {
    Serial.println("Logger init failed!");
    return;
  }

  logger.println("Boot complete — free heap: %lu", (unsigned long)ESP.getFreeHeap());
  Serial.println("Logger ready. Logging every 5 seconds.");
}

int counter = 0;

void loop() {
  counter++;
  logger.println("Heartbeat #%d — uptime %lu ms", counter, (unsigned long)millis());

  // Dump log every 10 entries
  if (counter % 10 == 0) {
    Serial.println("--- LOG DUMP ---");
    logger.dump(Serial);
    Serial.println("--- END DUMP ---");
    Serial.printf("Log size: %lu / %lu bytes, ~%lu lines\n",
                  (unsigned long)logger.size(),
                  (unsigned long)logger.capacity(),
                  (unsigned long)logger.lineCount());
  }

  delay(5000);
}
