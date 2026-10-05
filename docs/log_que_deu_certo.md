ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0xb (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x4bc
load:0x403c9700,len:0xbd8
load:0x403cc700,len:0x2a0c
entry 0x403c98d0
BeeCooler: diagnostico completo S3 | ciclo=60000 ms | ACK timeout=2000 ms
Gateway correspondente: comunicacao_wroom_test. Sem sleep ou cloud.
[PINOS] SHT int SDA/SCL=8/9 ext=17/18 | SPI SCK/MISO/MOSI=12/13/11 CS ADXL/SD=10/14
[PINOS] RTC CE/IO/CLK=47/40/21 | VBAT=4 | LoRa RX/TX/M0/M1/AUX=39/38/1/2/5
node=1 boot_id=31
[RTC] ajuste com hora da compilacao: FALHA na leitura de retorno (hora aproximada, sem NTP)
[RTC RAW] seg=00 min=00 hora=00 dia=00 mes=00 semana=00 ano=00 controle=00 | CH=0 modo12h=0 WP=0
[RTC] conferir RST/CE->GPIO47 DAT/IO->GPIO40 CLK->GPIO21, VCC e GND comum
[  1700][E][Preferences.cpp:50] begin(): nvs_open failed: NOT_FOUND
[LoRa] UART ESP<->modulo=9600 baud | monitor USB=115200 baud
[LoRa] SWITCH=0 confirmado na bancada: M0/M1 como entradas no ESP; controle por AT
[LoRa AT] consultando modulo em 9600 baud; sem gravar parametros
[LoRa AT TX] +++<CR><LF>
[LoRa AT RX] Entry AT

[LoRa AT TX] AT<CR><LF>
[LoRa AT RX] OK

[LoRa UART] OK: modulo respondeu AT; TX e RX locais responderam
[LoRa AT TX] AT+BAUD
[LoRa AT RX] +BAUD=3

[LoRa AT TX] AT+MODE
[LoRa AT RX] +MODE=0

[LoRa AT TX] AT+LEVEL
[LoRa AT RX] +LEVEL=2

[LoRa AT TX] AT+CHANNEL
[LoRa AT RX] +CHANNEL=41

[LoRa AT TX] AT+SLEEP
[LoRa AT RX] +SLEEP=2

[LoRa AT TX] AT+SWITCH
[LoRa AT RX] +SWITCH=0

[LoRa AT TX] AT+DRSSI
[LoRa AT RX] +DRSSI=0

[LoRa AT TX] AT+OPENKEY
[LoRa AT RX] +OPENKEY=1

[LoRa AT TX] AT+HELP
[LoRa AT RX] ===================================
LoRa Parameter:
+VERSION=V1.2.4
MODE:0
LEVEL:2 >> 2149bps
SLEEP:2
Frequency:915150000hz >> 41
MAC:ff,ff
CRC:1(true)
IQ:1(true)
Power:22dBm
===================================

[LoRa AT] saindo do modo AT para retomar transmissao
[LoRa AT TX] +++<CR><LF>
[LoRa AT RX] Exit AT
Power on

[LoRa] inicializacao local nao prova enlace RF; somente ACK confirma comunicacao.

========== DIAGNOSTICO 1 ==========
[RTC] FALHA: calendario invalido, parado ou sem resposta
[RTC RAW] seg=00 min=00 hora=00 dia=00 mes=00 semana=00 ano=00 controle=00 | CH=0 modo12h=0 WP=0
[RTC] conferir RST/CE->GPIO47 DAT/IO->GPIO40 CLK->GPIO21, VCC e GND comum
[BATERIA] leitura=4.583 V | divisor=5.000 | calibracao=1.0000
          Conferir com multimetro; ADC sozinho nao confirma sensor/calibracao.
[CHRG] GPIO15=HIGH (LOW indica carga; HIGH pode ser desconectado)
[SHT30 INTERNO] OK: 29.32 C | 48.10 %RH
[SHT30 EXTERNO] OK: 29.16 C | 50.18 %RH
[adxl] capturando janela de 10 s...
[ADXL345] DEVID=0xE5 | amostras=16000/16000 por eixo | duracao=10135 ms | overruns=0
[ADXL345] primeira amostra: X=-12 Y=240 Z=-78 LSB | -0.0468 0.9360 -0.3042 g
[VIBRACAO] RMS=31.98 mg | pico=279.34 mg | segmentos=61
           50-100 Hz: 17.09 dB
           100-200 Hz: 21.50 dB
           200-350 Hz: 23.13 dB
           350-600 Hz: 25.39 dB
[SD] detectado: tipo=3 capacidade=29818 MB
[SD] OK: escrita + fechamento + reabertura + leitura + comparacao (24 bytes)
[rec] seq=1 ts=7 t_in=2932 rh_in=4810 t_out=2916 rh_out=5018 vbat=4583 rms=320 peak=2793 bands=234,243,246,251 flags=0x0010
[LoRa] tentativa 1/4: quadro=1 registro=1
[LoRa] TX local concluido; aguardando ACK do gateway...
[LoRa] OK: gateway recebeu registro 1 e respondeu ACK | tentativa=1 rtt=765702 us
[RESUMO] RTC=FALHA | SHT interno=OK | SHT externo=OK | ADXL=OK | SD=OK | LoRa=ACK OK
[LoRa] ciclos confirmados=1/1
[cycle] ativo por 11340 ms

========== DIAGNOSTICO 2 ==========
[RTC] FALHA: calendario invalido, parado ou sem resposta
[RTC RAW] seg=00 min=00 hora=00 dia=00 mes=00 semana=00 ano=00 controle=00 | CH=0 modo12h=0 WP=0
[RTC] conferir RST/CE->GPIO47 DAT/IO->GPIO40 CLK->GPIO21, VCC e GND comum
[BATERIA] leitura=0.970 V | divisor=5.000 | calibracao=1.0000
          Conferir com multimetro; ADC sozinho nao confirma sensor/calibracao.
[CHRG] GPIO15=HIGH (LOW indica carga; HIGH pode ser desconectado)
[SHT30 INTERNO] OK: 29.28 C | 48.04 %RH
[SHT30 EXTERNO] OK: 29.18 C | 50.13 %RH
[adxl] capturando janela de 10 s...
[ADXL345] DEVID=0xE5 | amostras=16000/16000 por eixo | duracao=10140 ms | overruns=0
[ADXL345] primeira amostra: X=-14 Y=234 Z=-82 LSB | -0.0546 0.9126 -0.3198 g
[VIBRACAO] RMS=31.95 mg | pico=296.04 mg | segmentos=61
           50-100 Hz: 17.05 dB
           100-200 Hz: 21.56 dB
           200-350 Hz: 23.10 dB
           350-600 Hz: 25.40 dB
[SD] detectado: tipo=3 capacidade=29818 MB
[SD] OK: escrita + fechamento + reabertura + leitura + comparacao (24 bytes)
[rec] seq=2 ts=67 t_in=2928 rh_in=4804 t_out=2918 rh_out=5013 vbat=970 rms=320 peak=2960 bands=234,243,246,251 flags=0x0010
[LoRa] tentativa 1/4: quadro=2 registro=2
[LoRa] TX local concluido; aguardando ACK do gateway...
[LoRa] OK: gateway recebeu registro 2 e respondeu ACK | tentativa=1 rtt=758111 us
[RESUMO] RTC=FALHA | SHT interno=OK | SHT externo=OK | ADXL=OK | SD=OK | LoRa=ACK OK
[LoRa] ciclos confirmados=2/2
[cycle] ativo por 11322 ms

========== DIAGNOSTICO 3 ==========
[RTC] FALHA: calendario invalido, parado ou sem resposta
[RTC RAW] seg=00 min=00 hora=00 dia=00 mes=00 semana=00 ano=00 controle=00 | CH=0 modo12h=0 WP=0
[RTC] conferir RST/CE->GPIO47 DAT/IO->GPIO40 CLK->GPIO21, VCC e GND comum
[BATERIA] leitura=1.133 V | divisor=5.000 | calibracao=1.0000
          Conferir com multimetro; ADC sozinho nao confirma sensor/calibracao.
[CHRG] GPIO15=HIGH (LOW indica carga; HIGH pode ser desconectado)
[SHT30 INTERNO] OK: 29.33 C | 47.93 %RH
[SHT30 EXTERNO] OK: 29.19 C | 50.04 %RH
[adxl] capturando janela de 10 s...
[ADXL345] DEVID=0xE5 | amostras=16000/16000 por eixo | duracao=10140 ms | overruns=0
[ADXL345] primeira amostra: X=-16 Y=238 Z=-78 LSB | -0.0624 0.9282 -0.3042 g
[VIBRACAO] RMS=31.39 mg | pico=253.20 mg | segmentos=61
           50-100 Hz: 16.96 dB
           100-200 Hz: 21.35 dB
           200-350 Hz: 23.07 dB
           350-600 Hz: 25.28 dB
[SD] detectado: tipo=3 capacidade=29818 MB
[SD] OK: escrita + fechamento + reabertura + leitura + comparacao (24 bytes)
[rec] seq=3 ts=127 t_in=2933 rh_in=4793 t_out=2919 rh_out=5004 vbat=1133 rms=314 peak=2532 bands=234,243,246,251 flags=0x0010
[LoRa] tentativa 1/4: quadro=3 registro=3
[LoRa] TX local concluido; aguardando ACK do gateway...
[LoRa] OK: gateway recebeu registro 3 e respondeu ACK | tentativa=1 rtt=762718 us
[RESUMO] RTC=FALHA | SHT interno=OK | SHT externo=OK | ADXL=OK | SD=OK | LoRa=ACK OK
[LoRa] ciclos confirmados=3/3
[cycle] ativo por 11333 ms

========== DIAGNOSTICO 4 ==========
[RTC] FALHA: calendario invalido, parado ou sem resposta
[RTC RAW] seg=00 min=00 hora=00 dia=00 mes=00 semana=00 ano=00 controle=00 | CH=0 modo12h=0 WP=0
[RTC] conferir RST/CE->GPIO47 DAT/IO->GPIO40 CLK->GPIO21, VCC e GND comum
[BATERIA] leitura=1.222 V | divisor=5.000 | calibracao=1.0000
          Conferir com multimetro; ADC sozinho nao confirma sensor/calibracao.
[CHRG] GPIO15=HIGH (LOW indica carga; HIGH pode ser desconectado)
[SHT30 INTERNO] OK: 29.34 C | 48.07 %RH
[SHT30 EXTERNO] OK: 29.18 C | 50.18 %RH
[adxl] capturando janela de 10 s...
[ADXL345] DEVID=0xE5 | amostras=16000/16000 por eixo | duracao=10140 ms | overruns=0
[ADXL345] primeira amostra: X=-14 Y=236 Z=-80 LSB | -0.0546 0.9204 -0.3120 g
[VIBRACAO] RMS=31.47 mg | pico=248.16 mg | segmentos=61
           50-100 Hz: 16.95 dB
           100-200 Hz: 21.26 dB
           200-350 Hz: 23.19 dB
           350-600 Hz: 25.27 dB
[SD] detectado: tipo=3 capacidade=29818 MB
[SD] OK: escrita + fechamento + reabertura + leitura + comparacao (24 bytes)
[rec] seq=4 ts=187 t_in=2934 rh_in=4807 t_out=2918 rh_out=5018 vbat=1222 rms=315 peak=2482 bands=234,243,246,251 flags=0x0010
[LoRa] tentativa 1/4: quadro=4 registro=4
[LoRa] TX local concluido; aguardando ACK do gateway...
[LoRa] OK: gateway recebeu registro 4 e respondeu ACK | tentativa=1 rtt=773541 us
[RESUMO] RTC=FALHA | SHT interno=OK | SHT externo=OK | ADXL=OK | SD=OK | LoRa=ACK OK
[LoRa] ciclos confirmados=4/4
[cycle] ativo por 11334 ms

========== DIAGNOSTICO 5 ==========
[RTC] FALHA: calendario invalido, parado ou sem resposta
[RTC RAW] seg=00 min=00 hora=00 dia=00 mes=00 semana=00 ano=00 controle=00 | CH=0 modo12h=0 WP=0
[RTC] conferir RST/CE->GPIO47 DAT/IO->GPIO40 CLK->GPIO21, VCC e GND comum
[BATERIA] leitura=0.939 V | divisor=5.000 | calibracao=1.0000
          Conferir com multimetro; ADC sozinho nao confirma sensor/calibracao.
[CHRG] GPIO15=HIGH (LOW indica carga; HIGH pode ser desconectado)
[SHT30 INTERNO] OK: 29.30 C | 47.86 %RH
[SHT30 EXTERNO] OK: 29.19 C | 49.96 %RH
[adxl] capturando janela de 10 s...
[ADXL345] DEVID=0xE5 | amostras=16000/16000 por eixo | duracao=10140 ms | overruns=0
[ADXL345] primeira amostra: X=-12 Y=234 Z=-72 LSB | -0.0468 0.9126 -0.2808 g
[VIBRACAO] RMS=39.41 mg | pico=670.57 mg | segmentos=61
           50-100 Hz: 19.80 dB
           100-200 Hz: 25.14 dB
           200-350 Hz: 25.89 dB
           350-600 Hz: 26.75 dB
[SD] detectado: tipo=3 capacidade=29818 MB
[SD] OK: escrita + fechamento + reabertura + leitura + comparacao (24 bytes)
[rec] seq=5 ts=247 t_in=2930 rh_in=4786 t_out=2919 rh_out=4996 vbat=939 rms=394 peak=6706 bands=240,250,252,253 flags=0x0010
[LoRa] tentativa 1/4: quadro=5 registro=5
[LoRa] TX local concluido; aguardando ACK do gateway...
[LoRa] OK: gateway recebeu registro 5 e respondeu ACK | tentativa=1 rtt=766003 us
[RESUMO] RTC=FALHA | SHT interno=OK | SHT externo=OK | ADXL=OK | SD=OK | LoRa=ACK OK
[LoRa] ciclos confirmados=5/5
[cycle] ativo por 11334 ms

========== DIAGNOSTICO 6 ==========
[RTC] FALHA: calendario invalido, parado ou sem resposta
[RTC RAW] seg=00 min=00 hora=00 dia=00 mes=00 semana=00 ano=00 controle=00 | CH=0 modo12h=0 WP=0
[RTC] conferir RST/CE->GPIO47 DAT/IO->GPIO40 CLK->GPIO21, VCC e GND comum
[BATERIA] leitura=0.988 V | divisor=5.000 | calibracao=1.0000
          Conferir com multimetro; ADC sozinho nao confirma sensor/calibracao.
[CHRG] GPIO15=HIGH (LOW indica carga; HIGH pode ser desconectado)
[SHT30 INTERNO] OK: 29.26 C | 48.00 %RH
[SHT30 EXTERNO] OK: 29.16 C | 50.19 %RH
[adxl] capturando janela de 10 s...
[ADXL345] DEVID=0xE5 | amostras=16000/16000 por eixo | duracao=10140 ms | overruns=0
[ADXL345] primeira amostra: X=-12 Y=240 Z=-78 LSB | -0.0468 0.9360 -0.3042 g
[VIBRACAO] RMS=232.97 mg | pico=30215.60 mg | segmentos=61
           50-100 Hz: 34.63 dB
           100-200 Hz: 38.44 dB
           200-350 Hz: 40.21 dB
           350-600 Hz: 42.41 dB
[SD] detectado: tipo=3 capacidade=29818 MB
[SD] OK: escrita + fechamento + reabertura + leitura + comparacao (24 bytes)
[rec] seq=6 ts=307 t_in=2926 rh_in=4800 t_out=2916 rh_out=5019 vbat=988 rms=2330 peak=65534 bands=254,254,254,254 flags=0x0010
[LoRa] tentativa 1/4: quadro=6 registro=6
[LoRa] TX local concluido; aguardando ACK do gateway...
[LoRa] OK: gateway recebeu registro 6 e respondeu ACK | tentativa=1 rtt=764337 us
[RESUMO] RTC=FALHA | SHT interno=OK | SHT externo=OK | ADXL=OK | SD=OK | LoRa=ACK OK
[LoRa] ciclos confirmados=6/6
[cycle] ativo por 11332 ms

# OBSERVAÇÃO: O RTC não estava funcionando corretamente porque a protoboard estava com mal contato. Depois que foi trocado para jumpers fêmeas diretamente no RTC, funcionou.