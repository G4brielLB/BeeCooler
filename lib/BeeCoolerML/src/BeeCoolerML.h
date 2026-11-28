#ifndef BEE_COOLER_ML_H
#define BEE_COOLER_ML_H

// TinyML do no sensor (variante sensor_ml): previsao de T_in em +30 min,
// sensor virtual de T_in e decisao da nebulizacao. Os pesos e a matematica
// ficam em beecooler_tinyml.h, GERADO por analysis/ml/run_all.py; esta camada
// so traduz o registro canonico e trata sensor invalido, lacuna e relogio.
//
// Sem Arduino: compila no host (pio test -e native -f test_tinyml).
// Detalhes e limites: docs/tinyml_pipeline_v0.md.

#include <stdint.h>

namespace BeeCoolerML {

// Estado opaco que vive na RTC RAM (RTC_DATA_ATTR) entre um ciclo e outro.
// O tamanho real (bcml_state_t) e conferido em BeeCoolerML.cpp.
struct State {
  uint32_t words[32];
};

enum Reason : uint8_t {
  kOk = 0,            // previsao e decisao normais
  kWarmup = 1,        // EMAs aquecendo (< 60 min apos reset): decide so pela T_in
  kNight = 2,         // fora da janela diurna: estado atualizado, sem inferencia
  kOutInvalid = 3,    // sensor externo invalido: decide so pela T_in medida
  kInVirtual = 4,     // sensor interno invalido: T_in substituida pelo sensor virtual
  kNoSensor = 5,      // os dois sensores invalidos: bomba desligada
  kNoClock = 6,       // relogio invalido: sem hora local, bomba desligada
};

struct Result {
  bool pump;          // true = pedir nebulizacao
  bool model_ok;      // previsao usada na decisao
  uint8_t reason;     // Reason
  float t_in_used;    // T_in medida ou estimada (C)
  float t_pred;       // T_in prevista em +horizonte (C); = t_in_used sem modelo
  float t_virtual;    // T_in do sensor virtual (C); NAN se o externo e invalido
  float hour_local;
};

// Zera o estado (cold boot ou lacuna longa).
void reset(State& state);

// Um passo por registro de 10 min. Campos no formato do RecordV1
// (0,01 C / 0,01 %, sentinelas 0x8000 / 0xFFFF).
void step(State& state, uint32_t ts_utc, bool rtc_valid, int16_t t_in_c, uint16_t rh_in_c,
          int16_t t_out_c, uint16_t rh_out_c, int utc_offset_h, Result& out);

// Mesma decisao a partir de valores ja em C / %, usada pelo teste nativo.
void stepFloat(State& state, uint32_t ts_utc, bool rtc_valid, float t_in, bool in_ok, float rh_in,
               float t_out, float rh_out, bool out_ok, int utc_offset_h, Result& out);

// Validade usada no QC do treino (config.QC): T em 5..60 C; RH_ext > 0,5 %.
bool insideValid(int16_t t_in_c, uint16_t rh_in_c);
bool outsideValid(int16_t t_out_c, uint16_t rh_out_c);

int horizonMinutes();
const char* reasonName(uint8_t reason);

}  // namespace BeeCoolerML

#endif  // BEE_COOLER_ML_H
