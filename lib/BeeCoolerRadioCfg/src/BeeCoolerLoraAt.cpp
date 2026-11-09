#include "BeeCoolerLoraAt.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace BeeCoolerLoraAt {

namespace {

// Velocidades anunciadas para o DX-LR32 (+4800 por seguranca na varredura).
constexpr uint32_t kScanBauds[] = {115200U, 9600U, 57600U, 38400U, 19200U,
                                   4800U,   2400U, 1200U,  128000U};

constexpr uint32_t kReplyTimeoutMs = 900U;
constexpr uint32_t kReplyIdleMs = 120U;
constexpr uint32_t kRebootMs = 1000U;  // "Exit AT" -> "Power on"

void logf(Print* log, const char* format, ...) {
  if (log == nullptr) return;
  char line[192];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  log->print(line);
}

void flushInput(HardwareSerial& uart) {
  while (uart.available() > 0) uart.read();
}

bool waitAuxIdle(const Port& port, uint32_t timeout_ms) {
  if (port.aux < 0) return true;
  const uint32_t start = millis();
  while (digitalRead(port.aux) == HIGH) {
    if (millis() - start > timeout_ms) return false;
    delay(1);
  }
  return true;
}

void openUart(Port& port, uint32_t baud) {
  port.uart->end();
  port.uart->begin(baud, SERIAL_8N1, port.rx, port.tx);
  delay(20);
  flushInput(*port.uart);
}

// Le ate `timeout_ms` ou ate `idle_ms` sem bytes depois do primeiro.
size_t readReply(HardwareSerial& uart, char* reply, size_t capacity,
                 uint32_t timeout_ms = kReplyTimeoutMs,
                 uint32_t idle_ms = kReplyIdleMs) {
  size_t length = 0;
  size_t received = 0;
  const uint32_t start = millis();
  uint32_t last = start;
  while (millis() - start < timeout_ms) {
    while (uart.available() > 0) {
      const int value = uart.read();
      if (value < 0) break;
      ++received;
      last = millis();
      if (length + 1U < capacity) {
        reply[length++] = value == 0 ? '?' : static_cast<char>(value);
      }
    }
    if (received > 0 && millis() - last >= idle_ms) break;
    delay(1);
  }
  reply[length] = '\0';
  return received;
}

void logReply(Print* log, const char* prefix, const char* reply) {
  if (log == nullptr) return;
  log->print(prefix);
  for (const char* p = reply; *p != '\0'; ++p) {
    const uint8_t c = static_cast<uint8_t>(*p);
    if (c == '\r') continue;
    if (c == '\n') {
      log->print(" | ");
    } else if (c >= 32U && c <= 126U) {
      log->write(c);
    } else {
      log->printf("<%02X>", c);
    }
  }
  log->println();
}

// Uma linha exatamente igual a `expected` (sem diferenciar maiusculas).
bool hasLine(const char* reply, const char* expected) {
  const size_t wanted = strlen(expected);
  while (*reply != '\0') {
    while (*reply == '\r' || *reply == '\n') ++reply;
    const char* end = reply;
    while (*end != '\0' && *end != '\r' && *end != '\n') ++end;
    if (static_cast<size_t>(end - reply) == wanted &&
        strncasecmp(reply, expected, wanted) == 0) {
      return true;
    }
    reply = end;
  }
  return false;
}

// strcasestr portatil (nao depender de extensao GNU da newlib).
const char* findCi(const char* haystack, const char* needle) {
  const size_t n = strlen(needle);
  if (n == 0U) return haystack;
  for (const char* p = haystack; *p != '\0'; ++p) {
    if (strncasecmp(p, needle, n) == 0) return p;
  }
  return nullptr;
}

bool hasText(const char* reply, const char* text) {
  return findCi(reply, text) != nullptr;
}

// "+BAUD=3" -> 3
bool findValue(const char* reply, const char* name, int& value) {
  char key[20];
  snprintf(key, sizeof(key), "+%s=", name);
  const char* p = findCi(reply, key);
  if (p == nullptr) return false;
  p += strlen(key);
  char* end = nullptr;
  const long v = strtol(p, &end, 10);
  if (end == p) return false;
  value = static_cast<int>(v);
  return true;
}

void sendLine(HardwareSerial& uart, const char* command) {
  uart.print(command);
  uart.print("\r\n");
  uart.flush();
}

bool command(Port& port, const char* text, char* reply, size_t capacity,
             Print* log, uint32_t timeout_ms = kReplyTimeoutMs) {
  flushInput(*port.uart);
  sendLine(*port.uart, text);
  const size_t n = readReply(*port.uart, reply, capacity, timeout_ms);
  if (log != nullptr) {
    char prefix[40];
    snprintf(prefix, sizeof(prefix), "[LoRa AT] %s -> ", text);
    logReply(log, prefix, n > 0 ? reply : "(sem resposta)");
  }
  return n > 0;
}

struct AtSession {
  bool active;
  bool by_pins;  // entrou pelos pinos (SWITCH=1: M0=0, M1=1)
};

bool enterAt(Port& port, AtSession& session, Print* log) {
  HardwareSerial& uart = *port.uart;
  char reply[160];
  session = {false, false};
  waitAuxIdle(port, 500U);
  delay(150);  // silencio na UART antes do escape
  flushInput(uart);

  uart.print("+++\r\n");
  uart.flush();
  readReply(uart, reply, sizeof(reply), 1200U, 150U);
  if (hasLine(reply, "Exit AT")) {
    // Estava em AT (sessao anterior interrompida): saiu e reiniciou.
    delay(kRebootMs);
    flushInput(uart);
    uart.print("+++\r\n");
    uart.flush();
    readReply(uart, reply, sizeof(reply), 1200U, 150U);
  }
  if (hasLine(reply, "Entry AT")) {
    if (command(port, "AT", reply, sizeof(reply), nullptr) && hasLine(reply, "OK")) {
      session.active = true;
      return true;
    }
  }
  // Ja em AT sem o aviso?
  if (command(port, "AT", reply, sizeof(reply), nullptr) && hasLine(reply, "OK")) {
    session.active = true;
    return true;
  }
  // SWITCH=1: modo AT pelos pinos (M0 = 0, M1 = 1).
  if (port.pin_control && port.m0 >= 0 && port.m1 >= 0) {
    digitalWrite(port.m0, LOW);
    digitalWrite(port.m1, HIGH);
    delay(50);
    waitAuxIdle(port, 1000U);
    if (command(port, "AT", reply, sizeof(reply), nullptr) && hasLine(reply, "OK")) {
      session = {true, true};
      logf(log, "[LoRa AT] modo AT pelos pinos M0/M1\n");
      return true;
    }
    digitalWrite(port.m1, LOW);
    delay(50);
    waitAuxIdle(port, 1000U);
  }
  return false;
}

void exitAt(Port& port, AtSession& session, Print* log) {
  if (!session.active) return;
  HardwareSerial& uart = *port.uart;
  if (session.by_pins) {
    digitalWrite(port.m0, LOW);
    digitalWrite(port.m1, LOW);
    delay(50);
  } else {
    char reply[96];
    flushInput(uart);
    uart.print("+++\r\n");
    uart.flush();
    readReply(uart, reply, sizeof(reply), 1500U, 200U);
    if (log != nullptr) logReply(log, "[LoRa AT] saida -> ", reply);
    delay(kRebootMs);
  }
  waitAuxIdle(port, 1500U);
  flushInput(uart);
  session.active = false;
}

void readParams(Port& port, Config& c, Print* log) {
  struct Query {
    const char* name;
    int* value;
  };
  Query queries[] = {{"BAUD", &c.baud_code},  {"LEVEL", &c.level},
                     {"CHANNEL", &c.channel}, {"SLEEP", &c.sleep},
                     {"SWITCH", &c.switch_mode}, {"DRSSI", &c.drssi},
                     {"MODE", &c.mode}};
  char reply[160];
  char text[24];
  for (Query& q : queries) {
    snprintf(text, sizeof(text), "AT+%s", q.name);
    if (command(port, text, reply, sizeof(reply), log)) findValue(reply, q.name, *q.value);
  }
  char help[512];
  if (command(port, "AT+HELP", help, sizeof(help), nullptr, 1500U)) {
    const char* v = findCi(help, "+VERSION=");
    if (v != nullptr) {
      v += 9;
      size_t i = 0;
      while (i + 1U < sizeof(c.version) && v[i] != '\0' && v[i] != '\r' && v[i] != '\n') {
        c.version[i] = v[i];
        ++i;
      }
      c.version[i] = '\0';
    }
  }
  c.valid = c.baud_code != kUnknown && c.level != kUnknown && c.switch_mode != kUnknown;
}

// Escreve um parametro ("AT+X<n>", depois "AT+X=<n>"); true quando aceito.
bool writeParam(Port& port, const char* name, int value, Print* log) {
  char text[32];
  char reply[160];
  int echoed = kUnknown;
  const char* formats[] = {"AT+%s%d", "AT+%s=%d"};
  for (const char* format : formats) {
    snprintf(text, sizeof(text), format, name, value);
    command(port, text, reply, sizeof(reply), log);
    if (hasLine(reply, "OK") || (findValue(reply, name, echoed) && echoed == value)) {
      return true;
    }
  }
  return false;
}

bool verifyParam(Port& port, const char* name, int value, Print* log) {
  char text[24];
  char reply[160];
  int read_back = kUnknown;
  snprintf(text, sizeof(text), "AT+%s", name);
  return command(port, text, reply, sizeof(reply), log) && findValue(reply, name, read_back) &&
         read_back == value;
}

bool setParam(Port& port, const char* name, int value, Print* log) {
  if (writeParam(port, name, value, log) && verifyParam(port, name, value, log)) return true;
  logf(log, "[LoRa AT] FALHA: %s=%d nao confirmado\n", name, value);
  return false;
}

bool matches(const Config& c, const Target& t) {
  if (!c.valid) return false;
  if (t.uart_baud != 0U && c.uart_baud != t.uart_baud) return false;
  if (t.level != kUnknown && c.level != t.level) return false;
  if (t.switch_mode != kUnknown && c.switch_mode != t.switch_mode) return false;
  return true;
}

}  // namespace

void clearConfig(Config& c) {
  c.valid = false;
  c.uart_baud = 0U;
  c.baud_code = kUnknown;
  c.level = kUnknown;
  c.channel = kUnknown;
  c.sleep = kUnknown;
  c.switch_mode = kUnknown;
  c.drssi = kUnknown;
  c.mode = kUnknown;
  c.version[0] = '\0';
}

void applyPinMode(const Port& port) {
  if (port.m0 < 0 || port.m1 < 0) return;
  if (port.pin_control) {
    // Seguro so com SWITCH=1 (pinos do modulo sao entradas com pull-up fraco).
    pinMode(port.m0, OUTPUT);
    pinMode(port.m1, OUTPUT);
    digitalWrite(port.m0, LOW);
    digitalWrite(port.m1, LOW);
  } else {
    pinMode(port.m0, INPUT);
    pinMode(port.m1, INPUT);
  }
  if (port.aux >= 0) pinMode(port.aux, INPUT);
}

bool query(Port& port, uint32_t baud, Config& out, Print* log) {
  clearConfig(out);
  openUart(port, baud);
  AtSession session;
  if (!enterAt(port, session, nullptr)) {
    logf(log, "[LoRa AT] %lu Bd: sem resposta AT\n", static_cast<unsigned long>(baud));
    return false;
  }
  logf(log, "[LoRa AT] %lu Bd: modulo respondeu\n", static_cast<unsigned long>(baud));
  readParams(port, out, log);
  out.uart_baud = baud;
  exitAt(port, session, log);
  return out.valid;
}

bool detect(Port& port, uint32_t preferred, Config& out, Print* log) {
  if (preferred != 0U && query(port, preferred, out, log)) return true;
  for (uint32_t baud : kScanBauds) {
    if (baud == preferred) continue;
    if (query(port, baud, out, log)) return true;
  }
  clearConfig(out);
  return false;
}

Outcome provision(Port& port, const Target& target, Config& config, Print* log) {
  if (!config.valid) return Outcome::kFailed;
  if (matches(config, target)) return Outcome::kAlreadyOk;

  const bool need_level = target.level != kUnknown && config.level != target.level;
  const bool need_switch =
      target.switch_mode != kUnknown && config.switch_mode != target.switch_mode;
  const bool need_baud = target.uart_baud != 0U && config.uart_baud != target.uart_baud;
  logf(log,
       "[LoRa AT] reconfigurando (antes: %lu Bd codigo %d, LEVEL %d, SWITCH %d)%s%s%s\n",
       static_cast<unsigned long>(config.uart_baud), config.baud_code, config.level,
       config.switch_mode, need_level ? " LEVEL" : "", need_switch ? " SWITCH" : "",
       need_baud ? " BAUD" : "");

  bool ok = true;
  openUart(port, config.uart_baud);
  AtSession session;
  if (!enterAt(port, session, log)) return Outcome::kFailed;

  if (need_level) ok = setParam(port, "LEVEL", target.level, log) && ok;

  if (need_switch) {
    bool sw_ok = writeParam(port, "SWITCH", target.switch_mode, log);
    if (sw_ok && target.switch_mode == 1) {
      // Com SWITCH=1 os pinos do modulo viram entradas com pull-up fraco:
      // soltos, M0=M1=1 poe o modulo em sleep e ele para de responder.
      // Assume o controle ja em LOW/LOW, ANTES da leitura de volta. Se o
      // modulo so aplicar o SWITCH no reinicio, as duas saidas brigam ate la:
      // por isso o chamador so pede SWITCH=1 com resistores em serie em M0/M1.
      port.pin_control = true;
      applyPinMode(port);
    }
    sw_ok = sw_ok && verifyParam(port, "SWITCH", target.switch_mode, log);
    if (!sw_ok) logf(log, "[LoRa AT] FALHA: SWITCH=%d nao confirmado\n", target.switch_mode);
    if (target.switch_mode == 0 && sw_ok) {
      port.pin_control = false;
      applyPinMode(port);
    }
    ok = sw_ok && ok;
  }

  bool reached_baud = !need_baud;
  if (need_baud && target.baud_codes != nullptr) {
    for (size_t i = 0; i < target.baud_code_count && !reached_baud; ++i) {
      if (!session.active) {
        // Voltou de uma tentativa em outra velocidade: entra de novo.
        openUart(port, config.uart_baud);
        if (!enterAt(port, session, log)) break;
      }
      char text[24];
      char reply[96];
      snprintf(text, sizeof(text), "AT+BAUD%u", target.baud_codes[i]);
      const bool answered = command(port, text, reply, sizeof(reply), log);
      bool accepted = answered && hasLine(reply, "OK");
      if (answered && !accepted && !hasText(reply, "+BAUD")) {
        snprintf(text, sizeof(text), "AT+BAUD=%u", target.baud_codes[i]);
        const bool again = command(port, text, reply, sizeof(reply), log);
        accepted = again && hasLine(reply, "OK");
        if (again && !accepted) continue;  // recusado: nada mudou, ainda em AT
      }
      // Aceito, ou resposta ilegivel (a velocidade pode ja ter mudado):
      // sai do AT, o modulo reinicia, e a varredura diz onde ele ficou.
      exitAt(port, session, log);
      Config probe;
      if (!detect(port, target.uart_baud, probe, log)) break;
      config = probe;
      reached_baud = probe.uart_baud == target.uart_baud;
      if (!reached_baud) {
        logf(log, "[LoRa AT] codigo %u levou a %lu Bd; tentando o proximo\n",
             target.baud_codes[i], static_cast<unsigned long>(probe.uart_baud));
      }
    }
  }
  ok = ok && reached_baud;
  exitAt(port, session, log);

  // Conferencia final, comecando pela velocidade esperada.
  Config final_config;
  const uint32_t expected = reached_baud && target.uart_baud != 0U ? target.uart_baud
                                                                   : config.uart_baud;
  if (!detect(port, expected, final_config, log)) {
    clearConfig(config);
    return Outcome::kFailed;
  }
  config = final_config;
  port.pin_control = config.switch_mode == 1;
  applyPinMode(port);
  if (matches(config, target)) return Outcome::kChanged;
  (void)ok;
  return Outcome::kPartial;
}

void printConfig(const Config& c, Print& out) {
  if (!c.valid) {
    out.println("[LoRa] configuracao do modulo desconhecida (AT nao respondeu)");
    return;
  }
  out.printf("[LoRa] modulo %s: UART %lu Bd (BAUD=%d) LEVEL=%d CHANNEL=%d SLEEP=%d "
             "SWITCH=%d DRSSI=%d MODE=%d\n",
             c.version[0] != '\0' ? c.version : "?", static_cast<unsigned long>(c.uart_baud),
             c.baud_code, c.level, c.channel, c.sleep, c.switch_mode, c.drssi, c.mode);
}

const char* outcomeName(Outcome outcome) {
  switch (outcome) {
    case Outcome::kAlreadyOk:
      return "ja configurado";
    case Outcome::kChanged:
      return "reconfigurado e conferido";
    case Outcome::kPartial:
      return "PARCIAL (conferir o log AT)";
    default:
      return "FALHOU";
  }
}

}  // namespace BeeCoolerLoraAt
