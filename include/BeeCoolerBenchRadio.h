#pragma once

#include <Arduino.h>
#include <string.h>
#include <strings.h>

#include "BeeCoolerBenchConfig.h"

namespace BeeCoolerBench {

// Bench-only AT readback. Never saves parameters or resets factory settings.
// +++ toggles AT mode; exiting it reboots the module with its saved settings.
inline void readRadioReply(HardwareSerial& uart, char* reply, size_t capacity) {
  size_t length = 0;
  size_t received = 0;
  const uint32_t start = millis();
  uint32_t last_byte = start;
  while (millis() - start < 2500U) {
    while (uart.available() > 0) {
      const int value = uart.read();
      if (value < 0) break;
      ++received;
      last_byte = millis();
      if (length + 1U < capacity) reply[length++] = static_cast<char>(value);
    }
    if (received > 0 && millis() - last_byte >= 250U) break;
    delay(1);
  }
  reply[length] = '\0';
  if (received == 0) {
    Serial.println("[LoRa AT] SEM RESPOSTA");
  } else {
    Serial.print("[LoRa AT RX] ");
    for (size_t i = 0; i < length; ++i) {
      const uint8_t value = static_cast<uint8_t>(reply[i]);
      if (value == '\r' || value == '\n' || (value >= 32U && value <= 126U)) {
        Serial.write(value);
      } else {
        Serial.printf("<%02X>", value);
      }
    }
    Serial.println();
    if (received > length) Serial.println("[LoRa AT] resposta truncada no diagnostico");
  }
}

inline bool radioReplyHasLine(const char* reply, const char* expected) {
  const size_t wanted = strlen(expected);
  while (*reply != '\0') {
    while (*reply == '\r' || *reply == '\n') ++reply;
    const char* end = reply;
    while (*end != '\0' && *end != '\r' && *end != '\n') ++end;
    if (static_cast<size_t>(end - reply) == wanted &&
        strncasecmp(reply, expected, wanted) == 0) return true;
    reply = end;
  }
  return false;
}

inline void sendRadioCommand(HardwareSerial& uart, const char* command) {
  Serial.print("[LoRa AT TX] ");
  for (const char* p = command; *p != '\0'; ++p) {
    if (*p == '\r') Serial.print("<CR>");
    else if (*p == '\n') Serial.print("<LF>");
    else Serial.write(static_cast<uint8_t>(*p));
  }
  Serial.println();
  uart.print(command);
  uart.flush();
}

inline void toggleRadioAt(HardwareSerial& uart, char* reply, size_t capacity) {
  // Confirmed on the bench DX-LR32 V1.2.4: send the complete escape line.
  // A bare +++ failed; a boot banner must not prevent trying this format.
  sendRadioCommand(uart, "+++\r\n");
  readRadioReply(uart, reply, capacity);
}

inline bool probeRadio(HardwareSerial& uart) {
  char reply[640];
  Serial.printf("[LoRa AT] consultando modulo em %lu baud; sem gravar parametros\n",
                static_cast<unsigned long>(kLoraBaud));
  delay(1000);  // Allow module boot/mode transition and UART silence.
  if (uart.available() > 0) {
    Serial.println("[LoRa AT] dados anteriores aos comandos (boot/RF):");
    readRadioReply(uart, reply, sizeof(reply));
  }
  toggleRadioAt(uart, reply, sizeof(reply));
  if (radioReplyHasLine(reply, "Exit AT")) {
    // It was left in AT mode by an earlier interrupted bench run.
    delay(1000);
    toggleRadioAt(uart, reply, sizeof(reply));
  }
  const bool entered = radioReplyHasLine(reply, "Entry AT");
  sendRadioCommand(uart, "AT\r\n");
  readRadioReply(uart, reply, sizeof(reply));
  const bool uart_ok = radioReplyHasLine(reply, "OK");
  if (radioReplyHasLine(reply, "Power on")) {
    Serial.println("[LoRa AT] aviso de boot durante teste AT; nao equivale a resposta OK");
  }
  if (uart_ok) {
    Serial.println("[LoRa UART] OK: modulo respondeu AT; TX e RX locais responderam");
    const char* queries[] = {"AT+BAUD", "AT+MODE", "AT+LEVEL", "AT+CHANNEL",
                             "AT+SLEEP", "AT+SWITCH", "AT+DRSSI", "AT+OPENKEY",
                             "AT+HELP"};
    for (const char* query : queries) {
      Serial.printf("[LoRa AT TX] %s\n", query);
      uart.print(query);
      uart.print("\r\n");
      uart.flush();
      readRadioReply(uart, reply, sizeof(reply));
    }
  } else {
    Serial.println("[LoRa UART] NAO CONFIRMADO: AT sem OK; causa ainda indeterminada (procedimento AT, UART ou modulo)");
  }
  if (!entered && !uart_ok) {
    Serial.println("[LoRa AT] estado do modulo indeterminado; reiniciar alimentacao do modulo apos conferir ligacoes");
    return false;
  }
  Serial.println("[LoRa AT] saindo do modo AT para retomar transmissao");
  toggleRadioAt(uart, reply, sizeof(reply));
  const bool exited = radioReplyHasLine(reply, "Exit AT") ||
                      radioReplyHasLine(reply, "Power on");
  if (!exited) {
    Serial.println("[LoRa AT] FALHA: saida de AT nao confirmada; reiniciar alimentacao do modulo");
  }
  delay(1000);
  return uart_ok && exited;
}

}  // namespace BeeCoolerBench
