#include "Console.h"

#include <BeeCoolerLogic.h>
#include <BeeCoolerRecord.h>

#include "BatteryMonitor.h"
#include "NodeState.h"

namespace Console {

namespace {

void printTime(uint32_t epoch) {
  int y, mo, d, h, mi, s;
  BeeCoolerLogic::epochToCivil(epoch, y, mo, d, h, mi, s);
  Serial.printf("%lu = %04d-%02d-%02dT%02d:%02d:%02dZ\n",
                static_cast<unsigned long>(epoch), y, mo, d, h, mi, s);
}

String readLine() {
  String line;
  for (;;) {
    while (Serial.available() > 0) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\n' || c == '\r') {
        if (line.length() > 0) return line;
        continue;
      }
      line += c;
    }
    delay(5);
  }
}

void printRecord(uint32_t seq, const BeeCoolerRecord::RecordV1& r) {
  using namespace BeeCoolerRecord;
  Serial.printf("seq=%lu ts=%lu t_in=%d rh_in=%u t_out=%d rh_out=%u vbat=%u "
                "rms=%u peak=%u bands=%02X,%02X,%02X,%02X flags=0x%04X\n",
                static_cast<unsigned long>(seq), static_cast<unsigned long>(r.ts),
                r.t_in, r.rh_in, r.t_out, r.rh_out, r.vbat_mv, r.vib_rms,
                r.vib_peak, r.band_db[0], r.band_db[1], r.band_db[2],
                r.band_db[3], r.flags);
}

void help() {
  Serial.println("Comandos: help | status | time | settime <epoch> | vbat | "
                 "kcal <mV medido> | vdiv <valor> | dump [n] | exit");
}

}  // namespace

bool maybeRun(uint32_t window_ms, Ds1302Clock& clock, FlashRing& ring) {
  const uint32_t start = millis();
  while (Serial.available() == 0) {
    if (millis() - start > window_ms) return false;
    delay(10);
  }
  while (Serial.available() > 0) Serial.read();

  Serial.println("\n[BeeCooler] console de manutencao");
  help();
  for (;;) {
    Serial.print("> ");
    const String line = readLine();
    Serial.println(line);
    if (line == "exit") return true;
    if (line == "help") {
      help();
    } else if (line == "status") {
      Serial.printf("last_seq=%lu confirmed=%lu boot_count=%u power_level=%u "
                    "failed_sessions=%u sd_errors=%u kcal=%.4f vdiv=%.3f\n",
                    static_cast<unsigned long>(gState.last_seq),
                    static_cast<unsigned long>(gState.confirmed_seq),
                    gState.boot_count, gState.power.level, gState.failed_sessions,
                    gState.sd_errors, BatteryMonitor::gain(), BatteryMonitor::divider());
    } else if (line == "time") {
      uint32_t e = 0;
      if (clock.readEpoch(e)) {
        printTime(e);
      } else {
        Serial.println("DS1302 sem hora valida (Clock Halt)");
      }
    } else if (line.startsWith("settime ")) {
      const uint32_t e = static_cast<uint32_t>(line.substring(8).toInt());
      uint32_t build = 0;
      BeeCoolerLogic::parseBuildEpoch(__DATE__, __TIME__, build);
      if (e < build) {
        Serial.println("recusado: anterior a data de compilacao");
      } else {
        clock.writeEpoch(e);
        printTime(e);
      }
    } else if (line == "vbat") {
      Serial.printf("vbat = %u mV\n", BatteryMonitor::readMv());
    } else if (line.startsWith("kcal ")) {
      const long mv = line.substring(5).toInt();
      if (mv > 2000 && mv < 5000 &&
          BatteryMonitor::calibrateTo(static_cast<uint16_t>(mv))) {
        Serial.printf("K_CAL = %.4f, vbat = %u mV\n", BatteryMonitor::gain(),
                      BatteryMonitor::readMv());
      } else {
        Serial.println("valor invalido (2000..5000 mV)");
      }
    } else if (line.startsWith("vdiv ")) {
      const float v = line.substring(5).toFloat();
      if (v > 1.0f && v < 50.0f) {
        BatteryMonitor::setDivider(v);
        Serial.printf("DIV = %.3f\n", v);
      } else {
        Serial.println("valor invalido");
      }
    } else if (line.startsWith("dump")) {
      long n = line.length() > 5 ? line.substring(5).toInt() : 10;
      if (n < 1) n = 10;
      const uint32_t last = gState.last_seq;
      const uint32_t first = last > static_cast<uint32_t>(n) ? last - n + 1 : 1;
      for (uint32_t seq = first; seq <= last && last != 0; ++seq) {
        BeeCoolerRecord::RecordV1 r;
        if (ring.read(seq, r)) {
          printRecord(seq, r);
        } else {
          Serial.printf("seq=%lu slot invalido\n", static_cast<unsigned long>(seq));
        }
      }
    } else {
      help();
    }
  }
}

}  // namespace Console
