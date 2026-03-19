#include <LittleFS.h>
#include <PersistentLog.h>

PersistentLog logger(LittleFS, "/logs/system.log", 32768);

void setup() {
  Serial.begin(115200);
  delay(1000);

  LittleFS.begin(true);
  logger.begin();

  logger.println("Boot — reset reason: %d", (int)esp_reset_reason());

  Serial.println("Commands: DUMP, DUMP_LAST, CLEAR, STATUS, FILL");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "DUMP") {
      Serial.println("--- FULL LOG ---");
      logger.dump(Serial);
      Serial.println("--- END ---");
    }
    else if (cmd == "DUMP_LAST") {
      Serial.println("--- LAST 5 LINES ---");
      logger.dumpLast(Serial, 5);
      Serial.println("--- END ---");
    }
    else if (cmd == "CLEAR") {
      logger.clear();
      Serial.println("Log cleared.");
    }
    else if (cmd == "STATUS") {
      Serial.printf("Size: %lu / %lu bytes, ~%lu lines\n",
                    (unsigned long)logger.size(),
                    (unsigned long)logger.capacity(),
                    (unsigned long)logger.lineCount());
    }
    else if (cmd == "FILL") {
      Serial.println("Filling log to test rotation...");
      for (int i = 0; i < 500; i++) {
        logger.println("Fill entry #%d — padding to make lines longer for rotation testing", i);
      }
      Serial.printf("Done. Size: %lu / %lu bytes\n",
                    (unsigned long)logger.size(),
                    (unsigned long)logger.capacity());
    }
    else {
      logger.println("User command: %s", cmd.c_str());
      Serial.printf("Logged: %s\n", cmd.c_str());
    }
  }
}
