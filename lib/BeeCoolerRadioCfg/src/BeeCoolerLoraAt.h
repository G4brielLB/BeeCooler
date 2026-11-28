#ifndef BEE_COOLER_LORA_AT_H
#define BEE_COOLER_LORA_AT_H

// Leitura e configuracao do DX-LR32-900T22D por comandos AT, para os
// firmwares de producao (relatorio AP2, secao 8.2, passo 2).
//
// O que foi confirmado na bancada (docs/relatorio-achados-bancada-2026-10-03.md):
//   - "+++\r\n" alterna o modo AT ("Entry AT" / "Exit AT" + "Power on"); sair
//     do AT reinicia o modulo com os parametros gravados;
//   - consultas "AT+BAUD", "AT+LEVEL", ... respondem "+BAUD=3", "+LEVEL=2";
//   - fabrica: BAUD=3 (9600), LEVEL=2, CHANNEL=41, SLEEP=2, SWITCH=0, DRSSI=0;
//   - com SWITCH=0 os pinos M0/M1 sao SAIDAS do modulo.
//
// O que NAO foi confirmado no guia serial do LR32 e por isso e verificado por
// leitura de volta: a sintaxe de escrita ("AT+LEVEL3"; tenta "AT+LEVEL=3" se a
// primeira nao responder OK) e o codigo de BAUD de 115200 (tenta a lista dada
// pelo chamador e confere varrendo a UART depois do reinicio do modulo).
//
// Nunca altera canal, chave, potencia, MODE ou MAC.

#include <Arduino.h>

namespace BeeCoolerLoraAt {

constexpr int kUnknown = -1;

struct Port {
  HardwareSerial* uart;
  int8_t rx;
  int8_t tx;
  int8_t m0;
  int8_t m1;
  int8_t aux;
  // true somente com SWITCH=1 confirmado: o ESP comanda M0/M1. Com SWITCH=0
  // os pinos ficam como entrada no ESP (sao saidas do modulo).
  bool pin_control;
};

struct Config {
  bool valid;          // AT respondeu e BAUD, LEVEL e SWITCH foram lidos
  uint32_t uart_baud;  // velocidade da UART em que o AT respondeu
  int baud_code;
  int level;
  int channel;
  int sleep;
  int switch_mode;
  int drssi;
  int mode;
  char version[16];
};

struct Target {
  uint32_t uart_baud;         // 0 = manter
  int level;                  // kUnknown = manter
  int switch_mode;            // kUnknown = manter
  const uint8_t* baud_codes;  // codigos AT+BAUD a tentar para chegar a uart_baud
  size_t baud_code_count;
};

enum class Outcome : uint8_t { kAlreadyOk, kChanged, kPartial, kFailed };

void clearConfig(Config& config);

// M0/M1 em LOW/LOW (modo normal) quando port.pin_control; senao, entradas.
void applyPinMode(const Port& port);

// Abre a UART em `baud`, entra no AT, le a configuracao e sai do AT (o modulo
// reinicia, ~1 s). Deixa a UART aberta em `baud`.
bool query(Port& port, uint32_t baud, Config& out, Print* log);

// `preferred` primeiro, depois todas as velocidades suportadas pelo modulo.
// Em uma velocidade errada os bytes de escape podem sair pelo ar como lixo,
// que a central descarta pelo CRC32.
bool detect(Port& port, uint32_t preferred, Config& out, Print* log);

// Grava apenas os parametros diferentes do alvo e confere tudo no fim por uma
// nova deteccao. `config` entra com o estado atual e sai com o final;
// `port.pin_control` acompanha o SWITCH resultante.
Outcome provision(Port& port, const Target& target, Config& config, Print* log);

void printConfig(const Config& config, Print& out);
const char* outcomeName(Outcome outcome);

}  // namespace BeeCoolerLoraAt

#endif  // BEE_COOLER_LORA_AT_H
