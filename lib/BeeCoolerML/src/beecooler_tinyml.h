/* Gerado por BeeCooler/analysis/ml/run_all.py — NÃO EDITAR À MÃO.
 * Treino: dados de campo N01 (tiúba, quintal), registros utilizáveis após QC.
 * Limiares 🟡 provisórios (config.LABEL). */
#ifndef BEECOOLER_TINYML_H
#define BEECOOLER_TINYML_H
#include <stdint.h>
#include <string.h>
#include <math.h>

#define BCML_N_TAUS 4
static const float BCML_TAUS_MIN[BCML_N_TAUS] = {30.0f, 60.0f, 120.0f, 240.0f};
#define BCML_IDX_EMA60 1
#define BCML_IDX_EMA240 3
#define BCML_HORIZON_MIN 30
#define BCML_WARMUP_MIN 60.0f
#define BCML_T_ON 36.0f
#define BCML_T_OFF 35.0f
#define BCML_MIN_ON_MIN 20.0f
#define BCML_MIN_OFF_MIN 20.0f
#define BCML_DAY_H0 7.0f
#define BCML_DAY_H1 18.0f

/* FC: mlp 10-16-8-1, ReLU entre camadas, saída linear.
   Entradas (padronizadas com FC_MU/FC_SD): t_in, d_tin_10, d_tin_30, rh_in, t_out, d_tout_30, ema_out_60, ema_out_240, h_sin, h_cos */
#define FC_N_IN 10
#define FC_N_LAYERS 3
static const float FC_MU[10] = {3.42876663e+01f, -2.14992277e-02f, -2.09015980e-02f, 5.87804642e+01f, 3.45152397e+01f, -3.65018025e-02f, 3.47348328e+01f, 3.53900070e+01f, -7.20349103e-02f, 6.44613951e-02f};
static const float FC_SD[10] = {2.32070065e+00f, 1.64654315e-01f, 1.44665375e-01f, 5.28957558e+00f, 4.23136902e+00f, 3.24869305e-01f, 4.04636669e+00f, 3.29422998e+00f, 7.02119768e-01f, 7.05466926e-01f};
static const int FC_DIMS[4] = {10, 16, 8, 1};
static const float FC_W0[160] = {4.24493365e-02f, -3.05518787e-03f, 1.67614579e-01f, -5.80637008e-02f, -2.29741726e-02f, -2.60345526e-02f, 7.96605647e-02f, -3.27115618e-02f, -7.11788684e-02f, -7.40272552e-02f, -6.87165186e-02f, 2.33307655e-05f, -1.96488034e-02f, 6.51848502e-03f, -3.50595973e-02f, -1.41299829e-01f, -6.90051215e-03f, 1.72207225e-02f, -7.63258315e-04f, 6.94372356e-02f, 2.13322684e-01f, 4.76899706e-02f, -6.56696409e-02f, 1.60189837e-01f, -6.02387376e-02f, 1.57237917e-01f, 1.34242373e-02f, 7.40231888e-04f, 5.27757220e-02f, 4.38787392e-04f, 1.48651404e-02f, 4.02597412e-02f, -7.26293027e-02f, -2.00496968e-02f, -1.03460155e-01f, 7.01483637e-02f, 8.91089439e-02f, 4.80989255e-02f, 1.94748297e-01f, 1.46425873e-01f, 3.05806044e-02f, 1.53975114e-01f, 2.16915131e-01f, -1.34694646e-03f, -1.80341490e-02f, 1.16377394e-03f, 1.32950060e-02f, -4.41450579e-03f, 2.48571932e-02f, 9.57929716e-03f, 4.77134138e-02f, 6.44016415e-02f, 2.47800395e-01f, 2.70392187e-02f, -1.06462836e-01f, -4.92818188e-03f, 1.35008976e-01f, 7.84417465e-02f, 1.40000001e-01f, -6.57726370e-04f, 4.25636116e-03f, 8.30264296e-04f, 2.84784138e-02f, -2.44432427e-02f, -1.27334788e-01f, -4.10694405e-02f, 4.60466594e-02f, 5.68730896e-03f, 3.90349515e-02f, 1.57851055e-02f, 1.53855026e-01f, 1.52734469e-03f, 1.41957030e-01f, 5.53315096e-02f, 1.02146633e-01f, -1.21802492e-02f, -1.04774304e-01f, 3.26841301e-03f, -5.28225023e-03f, -1.43580288e-01f, -1.49532467e-01f, -1.56995468e-02f, -5.82078956e-02f, 1.13485970e-01f, 3.46935429e-02f, 1.54026188e-02f, -3.20251472e-02f, -6.22469522e-02f, 1.50400683e-01f, 1.70873761e-01f, 9.32713002e-02f, 7.21224816e-11f, -2.29519773e-02f, 1.26100975e-04f, 2.23158505e-02f, -1.91684514e-02f, -3.09247244e-02f, -6.26147836e-02f, 1.19346082e-01f, -2.92505678e-02f, -2.45163403e-03f, -5.10142790e-03f, 8.55219066e-02f, 6.09516120e-03f, -4.66843136e-02f, -1.68254953e-02f, -1.02449935e-02f, 2.75681377e-04f, -1.07436888e-01f, 7.41912704e-03f, -9.32562817e-03f, 7.35253021e-02f, -2.09473632e-02f, -3.15332934e-02f, 8.08006003e-02f, -4.77475598e-02f, 1.52714667e-03f, -1.77658703e-02f, 8.29314739e-02f, -4.22168933e-02f, -4.53590862e-02f, -8.38908106e-02f, -1.06156925e-02f, 3.19570026e-05f, -5.78765199e-02f, 4.99446935e-04f, -4.19147871e-02f, -7.42978156e-02f, 4.23260033e-02f, 3.13165858e-02f, -5.10660075e-02f, 1.31053515e-02f, 1.36927143e-02f, 3.96912452e-03f, -7.64698861e-03f, 1.21468455e-02f, 4.51673195e-02f, 1.96438581e-02f, 1.82616971e-02f, -3.29916002e-05f, 1.27035990e-01f, -5.55042061e-06f, 7.79611990e-03f, 1.43239558e-01f, 5.97985648e-02f, 2.30905283e-02f, -9.32789668e-02f, -2.75963843e-02f, -2.89475136e-02f, -1.38881514e-02f, -8.41416866e-02f, 2.15351693e-02f, -1.41272902e-01f, -6.41104802e-02f, -1.26010656e-01f, 8.72637518e-03f, 8.92386809e-02f, -8.18073284e-04f, -1.49274422e-02f, 5.33933751e-02f};
static const int8_t FC_W0_Q[160] = {22, -2, 86, -30, -12, -13, 41, -17, -36, -38, -35, 0, -10, 3, -18, -72, -4, 9, 0, 36, 109, 24, -34, 82, -31, 81, 7, 0, 27, 0, 8, 21, -37, -10, -53, 36, 46, 25, 100, 75, 16, 79, 111, -1, -9, 1, 7, -2, 13, 5, 24, 33, 127, 14, -55, -3, 69, 40, 72, 0, 2, 0, 15, -13, -65, -21, 24, 3, 20, 8, 79, 1, 73, 28, 52, -6, -54, 2, -3, -74, -77, -8, -30, 58, 18, 8, -16, -32, 77, 88, 48, 0, -12, 0, 11, -10, -16, -32, 61, -15, -1, -3, 44, 3, -24, -9, -5, 0, -55, 4, -5, 38, -11, -16, 41, -24, 1, -9, 43, -22, -23, -43, -5, 0, -30, 0, -21, -38, 22, 16, -26, 7, 7, 2, -4, 6, 23, 10, 9, 0, 65, 0, 4, 73, 31, 12, -48, -14, -15, -7, -43, 11, -72, -33, -65, 4, 46, 0, -8, 27};
static const float FC_B0[16] = {3.60382080e-01f, -1.56978965e-02f, 2.24912226e-01f, 7.59699583e-01f, 4.63285357e-01f, 1.67327046e-01f, -4.95413035e-01f, -1.69805020e-01f, 2.49133214e-01f, -1.57568187e-01f, 1.74041748e-01f, -4.59417164e-01f, -1.03078030e-01f, -2.77287006e-01f, 3.21808070e-01f, -1.69094428e-01f};
static const float FC_W1[128] = {5.04549405e-14f, 2.76091129e-01f, -8.16174299e-02f, -2.91783566e-04f, 1.41184609e-02f, -1.30482186e-15f, -5.23274578e-02f, -3.45002413e-02f, 8.60026848e-05f, 6.96253479e-02f, -2.26595532e-02f, 7.74916086e-16f, 4.33886237e-03f, 2.71542398e-14f, -5.17212041e-02f, -1.93121340e-02f, -1.54729732e-05f, 2.91957915e-01f, 8.11161101e-03f, -4.49542981e-09f, 1.26501350e-02f, 1.09579674e-12f, -1.25108823e-01f, -6.61854297e-02f, 2.96004439e-14f, 1.42729908e-01f, 1.29077107e-01f, -2.34270317e-07f, -6.65958375e-02f, -8.12302460e-05f, 1.73087835e-01f, 1.04193166e-01f, -6.98706629e-07f, 2.31731459e-01f, 3.91262800e-01f, 5.54006840e-11f, 1.49093363e-02f, -1.96831592e-12f, -5.86342365e-02f, 2.15280682e-01f, -4.02119981e-14f, -5.78562617e-02f, 6.30642399e-02f, 7.16972792e-08f, -4.89931274e-03f, 3.65688106e-06f, -7.00194985e-02f, 8.29933509e-02f, -1.87395551e-06f, 2.24467114e-01f, 2.21305713e-02f, -8.20864904e-12f, 2.45754849e-02f, 7.42401962e-13f, -4.04875726e-02f, -5.58646545e-02f, 1.22287968e-14f, -2.06383124e-01f, 4.89277840e-02f, -1.95322833e-15f, -1.11894561e-02f, -4.74705216e-16f, 8.00998807e-02f, 7.67778233e-02f, 5.02040668e-04f, -2.12297365e-01f, 1.50580317e-01f, -2.62010508e-06f, -1.49446875e-02f, -1.00826567e-17f, 1.59731776e-01f, 3.26557681e-02f, -9.94579864e-07f, -5.54993264e-02f, 1.30267203e-01f, 1.04755833e-04f, -1.17442049e-02f, -4.04872750e-12f, 2.48173818e-01f, 1.22612402e-01f, -1.86488580e-09f, -1.40837571e-02f, 2.46278107e-01f, -1.92685006e-13f, -1.63808186e-02f, -1.83267239e-05f, 1.06720701e-01f, 1.88983217e-01f, -2.40162318e-14f, 1.52584119e-02f, 1.91220477e-11f, 2.47276404e-15f, -1.63957273e-04f, -1.72201189e-16f, 3.46552990e-02f, 6.43731270e-04f, -1.11810941e-14f, 2.40803763e-01f, -5.25689460e-02f, 1.33061767e-04f, 1.27819451e-02f, -1.15416210e-09f, -2.73824800e-02f, -3.93391699e-02f, -1.98847449e-13f, 3.89029458e-03f, 5.39951316e-06f, -4.78680944e-04f, -2.69804474e-07f, -7.12574945e-07f, -2.39103829e-04f, -2.66291136e-06f, -2.19265781e-16f, 8.37973729e-02f, 5.70567288e-02f, -1.82411202e-06f, 1.55604323e-02f, 9.39085587e-10f, 4.74308990e-02f, 4.20451649e-02f, -2.07799440e-05f, 1.23955198e-01f, -3.48396599e-02f, 2.09149846e-04f, 7.55463494e-03f, 6.92986779e-11f, -5.23298085e-02f, -3.54936831e-02f};
static const int8_t FC_W1_Q[128] = {0, 90, -26, 0, 5, 0, -17, -11, 0, 23, -7, 0, 1, 0, -17, -6, 0, 95, 3, 0, 4, 0, -41, -21, 0, 46, 42, 0, -22, 0, 56, 34, 0, 75, 127, 0, 5, 0, -19, 70, 0, -19, 20, 0, -2, 0, -23, 27, 0, 73, 7, 0, 8, 0, -13, -18, 0, -67, 16, 0, -4, 0, 26, 25, 0, -69, 49, 0, -5, 0, 52, 11, 0, -18, 42, 0, -4, 0, 81, 40, 0, -5, 80, 0, -5, 0, 35, 61, 0, 5, 0, 0, 0, 0, 11, 0, 0, 78, -17, 0, 4, 0, -9, -13, 0, 1, 0, 0, 0, 0, 0, 0, 0, 27, 19, 0, 5, 0, 15, 14, 0, 40, -11, 0, 2, 0, -17, -12};
static const float FC_B1[8] = {-1.56721756e-01f, -2.42582977e-01f, -1.51956737e-01f, -4.29384381e-01f, 1.51553124e-01f, -4.88572538e-01f, 3.11095983e-01f, -2.64580905e-01f};
static const float FC_W2[8] = {-1.35172550e-02f, -5.54740191e-01f, 3.98779452e-01f, -3.92157926e-05f, -4.79942784e-02f, 4.03582400e-16f, 3.98926884e-01f, 3.36413562e-01f};
static const int8_t FC_W2_Q[8] = {-3, -127, 91, 0, -11, 0, 91, 77};
static const float FC_B2[1] = {-2.88040847e-01f};
static const float *const FC_W[3] = {FC_W0, FC_W1, FC_W2};
static const int8_t *const FC_WQ[3] = {FC_W0_Q, FC_W1_Q, FC_W2_Q};
static const float FC_WS[3] = {1.95118418e-03f, 3.08080944e-03f, 4.36803306e-03f};
static const float *const FC_B[3] = {FC_B0, FC_B1, FC_B2};

/* VS: linear 9-1, ReLU entre camadas, saída linear.
   Entradas (padronizadas com VS_MU/VS_SD): t_out, rh_out, ema_out_30, ema_out_60, ema_out_120, ema_out_240, ema_rh_out_60, h_sin, h_cos */
#define VS_N_IN 9
#define VS_N_LAYERS 1
static const float VS_MU[9] = {3.44623566e+01f, 4.80953751e+01f, 3.45582809e+01f, 3.46806145e+01f, 3.49220200e+01f, 3.53441887e+01f, 4.75946693e+01f, -6.28168657e-02f, 6.36824071e-02f};
static const float VS_SD[9] = {4.24783754e+00f, 1.27451477e+01f, 4.18107605e+00f, 4.06690502e+00f, 3.79888272e+00f, 3.31353068e+00f, 1.21652060e+01f, 7.05412984e-01f, 7.03129530e-01f};
static const int VS_DIMS[2] = {9, 1};
static const float VS_W0[9] = {-2.62579739e-01f, 3.58757526e-01f, 8.26136708e-01f, 1.56104159e+00f, 1.24169576e+00f, -2.43541881e-01f, 7.59714618e-02f, 2.37855330e-01f, 4.05160904e-01f};
static const int8_t VS_W0_Q[9] = {-21, 29, 67, 127, 101, -20, 6, 19, 33};
static const float VS_B0[1] = {3.42528191e+01f};
static const float *const VS_W[1] = {VS_W0};
static const int8_t *const VS_WQ[1] = {VS_W0_Q};
static const float VS_WS[1] = {1.22916662e-02f};
static const float *const VS_B[1] = {VS_B0};


/* ------------------------------------------------------------------ inferência */
#define BCML_MAX_W 32
static float bcml_mlp(const float *x, const float *mu, const float *sd, int n_layers, const int *dims,
                      const float *const *W, const int8_t *const *WQ, const float *WS, const float *const *B,
                      int use_int8) {
  float a[BCML_MAX_W], o[BCML_MAX_W];
  int l, i, j;
  for (i = 0; i < dims[0]; ++i) a[i] = (x[i] - mu[i]) / sd[i];
  for (l = 0; l < n_layers; ++l) {
    int n_in = dims[l], n_out = dims[l + 1];
    for (j = 0; j < n_out; ++j) {
      float acc = 0.0f;
      for (i = 0; i < n_in; ++i)
        acc += a[i] * (use_int8 ? (float)WQ[l][i * n_out + j] * WS[l] : W[l][i * n_out + j]);
      acc += B[l][j];
      o[j] = (l < n_layers - 1 && acc < 0.0f) ? 0.0f : acc;
    }
    for (j = 0; j < n_out; ++j) a[j] = o[j];
  }
  return a[0];
}

/* ------------------------------------------------------------------ estado (RTC RAM) */
typedef struct {
  uint32_t ts;          /* 0 = sem estado (reset/cold boot) */
  float ema_out[BCML_N_TAUS];
  float ema_rh;
  float tin[4], tout[4];
  uint32_t tsh[4];
  uint8_t n;            /* amostras no histórico (até 4) */
  uint32_t age_s;
  /* camada de decisão */
  uint8_t pump_on;
  uint32_t pump_since;
} bcml_state_t;

static void bcml_reset(bcml_state_t *s) { memset(s, 0, sizeof(*s)); }

static float bcml_slope(const bcml_state_t *s, const float *h, int k) {
  int last = s->n - 1, j;
  float dt;
  if (s->n < 2) return 0.0f;
  j = last - k; if (j < 0) j = 0;
  dt = (float)(s->tsh[last] - s->tsh[j]);
  return dt > 0.0f ? (h[last] - h[j]) / (dt / 600.0f) : 0.0f;
}

static void bcml_push(float *h, float v, uint8_t n) {
  int i;
  if (n < 4) { h[n] = v; return; }
  for (i = 0; i < 3; ++i) h[i] = h[i + 1];
  h[3] = v;
}

/* Atualiza o estado com um registro novo e preenche as features nomeadas. */
typedef struct { float t_in, rh_in, t_out, rh_out, ema_rh_out_60, d_tin_10, d_tin_30, d_tout_30, h_sin, h_cos, age_min;
                 float ema_out[BCML_N_TAUS]; } bcml_feat_t;

static void bcml_update(bcml_state_t *s, uint32_t ts, float t_in, float rh_in, float t_out, float rh_out,
                        float hour_local, bcml_feat_t *f) {
  int k;
  if (s->ts == 0) {
    for (k = 0; k < BCML_N_TAUS; ++k) s->ema_out[k] = t_out;
    s->ema_rh = rh_out;
  } else {
    float dt = (float)(ts - s->ts);
    s->age_s += (uint32_t)dt;
    for (k = 0; k < BCML_N_TAUS; ++k) s->ema_out[k] += (1.0f - expf(-dt / (BCML_TAUS_MIN[k] * 60.0f))) * (t_out - s->ema_out[k]);
    s->ema_rh += (1.0f - expf(-dt / 3600.0f)) * (rh_out - s->ema_rh);
  }
  s->ts = ts;
  bcml_push(s->tin, t_in, s->n); bcml_push(s->tout, t_out, s->n);
  { uint32_t *h = s->tsh; if (s->n < 4) h[s->n] = ts; else { h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = ts; } }
  if (s->n < 4) s->n++;
  f->t_in = t_in; f->rh_in = rh_in; f->t_out = t_out; f->rh_out = rh_out; f->ema_rh_out_60 = s->ema_rh;
  for (k = 0; k < BCML_N_TAUS; ++k) f->ema_out[k] = s->ema_out[k];
  f->d_tin_10 = bcml_slope(s, s->tin, 1); f->d_tin_30 = bcml_slope(s, s->tin, 3); f->d_tout_30 = bcml_slope(s, s->tout, 3);
  f->h_sin = sinf(2.0f * 3.14159265f * hour_local / 24.0f); f->h_cos = cosf(2.0f * 3.14159265f * hour_local / 24.0f);
  f->age_min = s->age_s / 60.0f;
}

/* Previsão de T_in em +BCML_HORIZON_MIN (°C). */
static float bcml_forecast(const bcml_feat_t *f, int use_int8) {
  float x[FC_N_IN] = {f->t_in, f->d_tin_10, f->d_tin_30, f->rh_in, f->t_out, f->d_tout_30,
                      f->ema_out[BCML_IDX_EMA60], f->ema_out[BCML_IDX_EMA240], f->h_sin, f->h_cos};
  return f->t_in + bcml_mlp(x, FC_MU, FC_SD, FC_N_LAYERS, FC_DIMS, FC_W, FC_WQ, FC_WS, FC_B, use_int8);
}

/* Sensor virtual: T_in estimada só com o sensor externo (°C). */
static float bcml_virtual_tin(const bcml_feat_t *f, int use_int8) {
  float x[VS_N_IN] = {f->t_out, f->rh_out, f->ema_out[0], f->ema_out[1], f->ema_out[2], f->ema_out[3], f->ema_rh_out_60,
                      f->h_sin, f->h_cos};
  return bcml_mlp(x, VS_MU, VS_SD, VS_N_LAYERS, VS_DIMS, VS_W, VS_WQ, VS_WS, VS_B, use_int8);
}

/* Camada de decisão (regras de segurança). Devolve 1 = nebulizar.
   Sem modelo confiável (aquecimento das EMAs), usa só a T_in medida. */
static uint8_t bcml_decide(bcml_state_t *s, uint32_t ts, float hour_local, float t_in, float t_pred, int model_ok) {
  float drive = (model_ok && t_pred > t_in) ? t_pred : t_in;
  float held = s->pump_since ? (float)(ts - s->pump_since) / 60.0f : 1e9f;
  if (hour_local < BCML_DAY_H0 || hour_local >= BCML_DAY_H1) {
    if (s->pump_on) { s->pump_on = 0; s->pump_since = ts; }
  } else if (!s->pump_on && drive >= BCML_T_ON && held >= BCML_MIN_OFF_MIN) {
    s->pump_on = 1; s->pump_since = ts;
  } else if (s->pump_on && drive <= BCML_T_OFF && held >= BCML_MIN_ON_MIN) {
    s->pump_on = 0; s->pump_since = ts;
  }
  return s->pump_on;
}

#endif /* BEECOOLER_TINYML_H */
