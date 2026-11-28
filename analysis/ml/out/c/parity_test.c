/* Teste de paridade C x Python:
   gcc -std=c99 -O2 -I../../../../lib/BeeCoolerML/src parity_test.c -lm -o parity && ./parity test_vectors.csv */
#include <stdio.h>
#include <stdlib.h>
#include "beecooler_tinyml.h"

int main(int argc, char **argv) {
  FILE *fp = fopen(argc > 1 ? argv[1] : "test_vectors.csv", "r");
  char line[512];
  bcml_state_t st; bcml_feat_t f;
  long seg_prev = -999, n = 0, n_bad = 0, n_pump_bad = 0;
  double max_fc = 0, max_vs = 0, max_fc_q = 0;
  if (!fp) { printf("sem arquivo\n"); return 2; }
  fgets(line, sizeof line, fp); /* cabeçalho */
  bcml_reset(&st);
  while (fgets(line, sizeof line, fp)) {
    long seg; unsigned long ts; float tin, rhin, tout, rhout, hour, e_fc, e_vs, e_fc_q; int e_pump;
    float pfc, pvs, pq; uint8_t pump; double d;
    if (sscanf(line, "%ld,%lu,%f,%f,%f,%f,%f,%f,%f,%f,%d", &seg, &ts, &tin, &rhin, &tout, &rhout, &hour,
               &e_fc, &e_vs, &e_fc_q, &e_pump) != 11) continue;
    if (seg != seg_prev) { bcml_reset(&st); seg_prev = seg; }
    bcml_update(&st, (uint32_t)ts, tin, rhin, tout, rhout, hour, &f);
    pfc = bcml_forecast(&f, 0); pvs = bcml_virtual_tin(&f, 0); pq = bcml_forecast(&f, 1);
    pump = bcml_decide(&st, (uint32_t)ts, hour, tin, pfc, f.age_min >= BCML_WARMUP_MIN);
    d = fabs(pfc - e_fc); if (d > max_fc) max_fc = d;
    d = fabs(pvs - e_vs); if (d > max_vs) max_vs = d;
    d = fabs(pq - e_fc_q); if (d > max_fc_q) max_fc_q = d;
    if (fabs(pfc - e_fc) > 1e-3 || fabs(pvs - e_vs) > 1e-3) n_bad++;
    if (pump != e_pump) n_pump_bad++;
    n++;
  }
  printf("amostras=%ld  max|dif| previsao=%.2e  sensor_virtual=%.2e  previsao_int8=%.2e  divergencias=%ld  bomba_divergente=%ld\n",
         n, max_fc, max_vs, max_fc_q, n_bad, n_pump_bad);
  return (n_bad == 0 && n_pump_bad == 0) ? 0 : 1;
}
