(.venv) gabriellopesbastos@MacBook-Air-de-Gabriel BeeCooler % /Users/gabriellopesbastos/.platformio/penv/bin/pio device monitor -p /dev/cu.usbserial-5B520925891 -b 115200
--- Terminal on /dev/cu.usbserial-5B520925891 | 115200 8-N-1
--- Available filters and text transformations: debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4
BeeCooler: teste de comunicacao WROOM (so serial, sem AWS)
[PINOS LoRa] RX=16 TX=17 M0=19 M1=21 AUX=18 | UART=9600 baud
[LoRa] UART ESP<->modulo=9600 por padrao; monitor USB=115200 baud
[LoRa] SWITCH=0 confirmado na bancada: M0/M1 como entradas no ESP; controle por AT
[LoRa GPIO] M0=1 M1=1 AUX=0 RX=1 | M0/M1 lidos, nao comandados; AUX=0 quando ocioso
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

[LoRa] Enlace ainda NAO CONFIRMADO; aguarde RX com CRC32 e ACK recebido no S3.
aguardando quadros LoRa...
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[stats] quadros_ok=0 crc_falhos=0 bytes_descartados=0 acks_tx=0 acks_falhos=0 registros=0 duplicados=0 uart_bytes=0
[LoRa] NAO CONFIRMADO: nenhum quadro valido; verificar S3 e configuracao dos radios.
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=78 quadro=8 tentativa=1
[rx] no=1 seq=8 tentativa=1 ts=4071104588
     RTC do S3: 2099-01-03 06:23:08 (sem sincronizacao externa)
     SHT30 interno: 28.19 C / 44.23 %RH
     SHT30 externo: 28.01 C / 46.74 %RH
     bateria medida=0.810 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=34.3 mg pico=277.3 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.50 22.50 24.00 25.50
BEEREC {"node":1,"seq":8,"attempt":1,"ts":4071104588,"t_in":2819,"rh_in":4423,"t_out":2801,"rh_out":4674,"vbat_mv":810,"vib_rms":343,"vib_peak":2773,"bands":[239,245,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=8 registro=8; conferir recepcao no S3
[stats] quadros_ok=1 crc_falhos=0 bytes_descartados=0 acks_tx=1 acks_falhos=0 registros=1 duplicados=0 uart_bytes=41
[stats] quadros_ok=1 crc_falhos=0 bytes_descartados=0 acks_tx=1 acks_falhos=0 registros=1 duplicados=0 uart_bytes=41
[stats] quadros_ok=1 crc_falhos=0 bytes_descartados=0 acks_tx=1 acks_falhos=0 registros=1 duplicados=0 uart_bytes=41
[stats] quadros_ok=1 crc_falhos=0 bytes_descartados=0 acks_tx=1 acks_falhos=0 registros=1 duplicados=0 uart_bytes=41
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=78 quadro=9 tentativa=1
[rx] no=1 seq=9 tentativa=1 ts=4071104648
     RTC do S3: 2099-01-03 06:24:08 (sem sincronizacao externa)
     SHT30 interno: 28.32 C / 44.81 %RH
     SHT30 externo: 28.13 C / 47.89 %RH
     bateria medida=0.817 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.5 mg pico=254.2 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":9,"attempt":1,"ts":4071104648,"t_in":2832,"rh_in":4481,"t_out":2813,"rh_out":4789,"vbat_mv":817,"vib_rms":335,"vib_peak":2542,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=9 registro=9; conferir recepcao no S3
[stats] quadros_ok=2 crc_falhos=0 bytes_descartados=0 acks_tx=2 acks_falhos=0 registros=2 duplicados=0 uart_bytes=82
[stats] quadros_ok=2 crc_falhos=0 bytes_descartados=0 acks_tx=2 acks_falhos=0 registros=2 duplicados=0 uart_bytes=82
[stats] quadros_ok=2 crc_falhos=0 bytes_descartados=0 acks_tx=2 acks_falhos=0 registros=2 duplicados=0 uart_bytes=82
[stats] quadros_ok=2 crc_falhos=0 bytes_descartados=0 acks_tx=2 acks_falhos=0 registros=2 duplicados=0 uart_bytes=82
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=78 quadro=10 tentativa=1
[rx] no=1 seq=10 tentativa=1 ts=4071104708
     RTC do S3: 2099-01-03 06:25:08 (sem sincronizacao externa)
     SHT30 interno: 28.35 C / 43.85 %RH
     SHT30 externo: 28.26 C / 46.41 %RH
     bateria medida=0.816 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.0 mg pico=282.1 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.00 22.00 23.50 25.50
BEEREC {"node":1,"seq":10,"attempt":1,"ts":4071104708,"t_in":2835,"rh_in":4385,"t_out":2826,"rh_out":4641,"vbat_mv":816,"vib_rms":330,"vib_peak":2821,"bands":[236,244,247,251],"flags":0}
[ACK] TX local OK: no=1 quadro=10 registro=10; conferir recepcao no S3
[stats] quadros_ok=3 crc_falhos=0 bytes_descartados=0 acks_tx=3 acks_falhos=0 registros=3 duplicados=0 uart_bytes=123
[stats] quadros_ok=3 crc_falhos=0 bytes_descartados=0 acks_tx=3 acks_falhos=0 registros=3 duplicados=0 uart_bytes=123
[stats] quadros_ok=3 crc_falhos=0 bytes_descartados=0 acks_tx=3 acks_falhos=0 registros=3 duplicados=0 uart_bytes=123
[stats] quadros_ok=3 crc_falhos=0 bytes_descartados=0 acks_tx=3 acks_falhos=0 registros=3 duplicados=0 uart_bytes=123
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=78 quadro=11 tentativa=1
[rx] no=1 seq=11 tentativa=1 ts=4071104768
     RTC do S3: 2099-01-03 06:26:08 (sem sincronizacao externa)
     SHT30 interno: 28.37 C / 43.58 %RH
     SHT30 externo: 28.35 C / 46.01 %RH
     bateria medida=0.811 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.9 mg pico=298.6 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.50 22.00 24.00 25.50
BEEREC {"node":1,"seq":11,"attempt":1,"ts":4071104768,"t_in":2837,"rh_in":4358,"t_out":2835,"rh_out":4601,"vbat_mv":811,"vib_rms":339,"vib_peak":2986,"bands":[239,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=11 registro=11; conferir recepcao no S3
[stats] quadros_ok=4 crc_falhos=0 bytes_descartados=0 acks_tx=4 acks_falhos=0 registros=4 duplicados=0 uart_bytes=164
[stats] quadros_ok=4 crc_falhos=0 bytes_descartados=0 acks_tx=4 acks_falhos=0 registros=4 duplicados=0 uart_bytes=164
[stats] quadros_ok=4 crc_falhos=0 bytes_descartados=0 acks_tx=4 acks_falhos=0 registros=4 duplicados=0 uart_bytes=164
[stats] quadros_ok=4 crc_falhos=0 bytes_descartados=0 acks_tx=4 acks_falhos=0 registros=4 duplicados=0 uart_bytes=164
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=78 quadro=12 tentativa=1
[rx] no=1 seq=12 tentativa=1 ts=4071104828
     RTC do S3: 2099-01-03 06:27:08 (sem sincronizacao externa)
     SHT30 interno: 28.39 C / 43.68 %RH
     SHT30 externo: 28.42 C / 45.92 %RH
     bateria medida=0.808 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.5 mg pico=270.8 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":12,"attempt":1,"ts":4071104828,"t_in":2839,"rh_in":4368,"t_out":2842,"rh_out":4592,"vbat_mv":808,"vib_rms":335,"vib_peak":2708,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=12 registro=12; conferir recepcao no S3
[stats] quadros_ok=5 crc_falhos=0 bytes_descartados=0 acks_tx=5 acks_falhos=0 registros=5 duplicados=0 uart_bytes=205
[stats] quadros_ok=5 crc_falhos=0 bytes_descartados=0 acks_tx=5 acks_falhos=0 registros=5 duplicados=0 uart_bytes=205
[stats] quadros_ok=5 crc_falhos=0 bytes_descartados=0 acks_tx=5 acks_falhos=0 registros=5 duplicados=0 uart_bytes=205
[stats] quadros_ok=5 crc_falhos=0 bytes_descartados=0 acks_tx=5 acks_falhos=0 registros=5 duplicados=0 uart_bytes=205
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=78 quadro=13 tentativa=1
[rx] no=1 seq=13 tentativa=1 ts=4071104888
     RTC do S3: 2099-01-03 06:28:08 (sem sincronizacao externa)
     SHT30 interno: 28.43 C / 43.57 %RH
     SHT30 externo: 28.50 C / 45.71 %RH
     bateria medida=0.815 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.4 mg pico=275.0 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":13,"attempt":1,"ts":4071104888,"t_in":2843,"rh_in":4357,"t_out":2850,"rh_out":4571,"vbat_mv":815,"vib_rms":334,"vib_peak":2750,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=13 registro=13; conferir recepcao no S3
[stats] quadros_ok=6 crc_falhos=0 bytes_descartados=0 acks_tx=6 acks_falhos=0 registros=6 duplicados=0 uart_bytes=246
[stats] quadros_ok=6 crc_falhos=0 bytes_descartados=0 acks_tx=6 acks_falhos=0 registros=6 duplicados=0 uart_bytes=246
[stats] quadros_ok=6 crc_falhos=0 bytes_descartados=0 acks_tx=6 acks_falhos=0 registros=6 duplicados=0 uart_bytes=246
[stats] quadros_ok=6 crc_falhos=0 bytes_descartados=0 acks_tx=6 acks_falhos=0 registros=6 duplicados=0 uart_bytes=246
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=78 quadro=14 tentativa=1
[rx] no=1 seq=14 tentativa=1 ts=4071104948
     RTC do S3: 2099-01-03 06:29:08 (sem sincronizacao externa)
     SHT30 interno: 28.37 C / 43.25 %RH
     SHT30 externo: 28.50 C / 45.31 %RH
     bateria medida=0.814 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=34.1 mg pico=252.7 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.50 22.00 24.00 26.00
BEEREC {"node":1,"seq":14,"attempt":1,"ts":4071104948,"t_in":2837,"rh_in":4325,"t_out":2850,"rh_out":4531,"vbat_mv":814,"vib_rms":341,"vib_peak":2527,"bands":[237,244,248,252],"flags":0}
[ACK] TX local OK: no=1 quadro=14 registro=14; conferir recepcao no S3
[stats] quadros_ok=7 crc_falhos=0 bytes_descartados=0 acks_tx=7 acks_falhos=0 registros=7 duplicados=0 uart_bytes=287
[stats] quadros_ok=7 crc_falhos=0 bytes_descartados=0 acks_tx=7 acks_falhos=0 registros=7 duplicados=0 uart_bytes=287
[stats] quadros_ok=7 crc_falhos=0 bytes_descartados=0 acks_tx=7 acks_falhos=0 registros=7 duplicados=0 uart_bytes=287
[stats] quadros_ok=7 crc_falhos=0 bytes_descartados=0 acks_tx=7 acks_falhos=0 registros=7 duplicados=0 uart_bytes=287
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=78 quadro=15 tentativa=1
[rx] no=1 seq=15 tentativa=1 ts=4071105008
     RTC do S3: 2099-01-03 06:30:08 (sem sincronizacao externa)
     SHT30 interno: 28.35 C / 43.08 %RH
     SHT30 externo: 28.50 C / 45.09 %RH
     bateria medida=0.800 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.5 mg pico=305.2 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":15,"attempt":1,"ts":4071105008,"t_in":2835,"rh_in":4308,"t_out":2850,"rh_out":4509,"vbat_mv":800,"vib_rms":335,"vib_peak":3052,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=15 registro=15; conferir recepcao no S3
[stats] quadros_ok=8 crc_falhos=0 bytes_descartados=0 acks_tx=8 acks_falhos=0 registros=8 duplicados=0 uart_bytes=328
[stats] quadros_ok=8 crc_falhos=0 bytes_descartados=0 acks_tx=8 acks_falhos=0 registros=8 duplicados=0 uart_bytes=328
[stats] quadros_ok=8 crc_falhos=0 bytes_descartados=0 acks_tx=8 acks_falhos=0 registros=8 duplicados=0 uart_bytes=328
[stats] quadros_ok=8 crc_falhos=0 bytes_descartados=0 acks_tx=8 acks_falhos=0 registros=8 duplicados=0 uart_bytes=328
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=78 quadro=16 tentativa=1
[rx] no=1 seq=16 tentativa=1 ts=4071105068
     RTC do S3: 2099-01-03 06:31:08 (sem sincronizacao externa)
     SHT30 interno: 28.27 C / 42.73 %RH
     SHT30 externo: 28.49 C / 44.72 %RH
     bateria medida=0.809 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.0 mg pico=256.5 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":16,"attempt":1,"ts":4071105068,"t_in":2827,"rh_in":4273,"t_out":2849,"rh_out":4472,"vbat_mv":809,"vib_rms":330,"vib_peak":2565,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=16 registro=16; conferir recepcao no S3
[stats] quadros_ok=9 crc_falhos=0 bytes_descartados=0 acks_tx=9 acks_falhos=0 registros=9 duplicados=0 uart_bytes=369
[stats] quadros_ok=9 crc_falhos=0 bytes_descartados=0 acks_tx=9 acks_falhos=0 registros=9 duplicados=0 uart_bytes=369
[stats] quadros_ok=9 crc_falhos=0 bytes_descartados=1 acks_tx=9 acks_falhos=0 registros=9 duplicados=0 uart_bytes=370
[stats] quadros_ok=9 crc_falhos=0 bytes_descartados=1 acks_tx=9 acks_falhos=0 registros=9 duplicados=0 uart_bytes=370
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=1 tentativa=1
[rx] no=1 seq=1 tentativa=1 ts=4071105123
     RTC do S3: 2099-01-03 06:32:03 (sem sincronizacao externa)
     SHT30 interno: 28.32 C / 42.90 %RH
     SHT30 externo: 28.50 C / 44.82 %RH
     bateria medida=1.657 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=34.7 mg pico=278.4 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 23.00 24.00 25.50
BEEREC {"node":1,"seq":1,"attempt":1,"ts":4071105123,"t_in":2832,"rh_in":4290,"t_out":2850,"rh_out":4482,"vbat_mv":1657,"vib_rms":347,"vib_peak":2784,"bands":[238,246,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=1 registro=1; conferir recepcao no S3
[stats] quadros_ok=10 crc_falhos=0 bytes_descartados=1 acks_tx=10 acks_falhos=0 registros=10 duplicados=0 uart_bytes=411
[stats] quadros_ok=10 crc_falhos=0 bytes_descartados=1 acks_tx=10 acks_falhos=0 registros=10 duplicados=0 uart_bytes=411
[stats] quadros_ok=10 crc_falhos=0 bytes_descartados=1 acks_tx=10 acks_falhos=0 registros=10 duplicados=0 uart_bytes=411
[stats] quadros_ok=10 crc_falhos=0 bytes_descartados=1 acks_tx=10 acks_falhos=0 registros=10 duplicados=0 uart_bytes=411
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=2 tentativa=1
[rx] no=1 seq=2 tentativa=1 ts=4071105183
     RTC do S3: 2099-01-03 06:33:03 (sem sincronizacao externa)
     SHT30 interno: 28.34 C / 42.84 %RH
     SHT30 externo: 28.54 C / 44.77 %RH
     bateria medida=0.816 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.3 mg pico=255.7 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":2,"attempt":1,"ts":4071105183,"t_in":2834,"rh_in":4284,"t_out":2854,"rh_out":4477,"vbat_mv":816,"vib_rms":333,"vib_peak":2557,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=2 registro=2; conferir recepcao no S3
[stats] quadros_ok=11 crc_falhos=0 bytes_descartados=1 acks_tx=11 acks_falhos=0 registros=11 duplicados=0 uart_bytes=452
[stats] quadros_ok=11 crc_falhos=0 bytes_descartados=1 acks_tx=11 acks_falhos=0 registros=11 duplicados=0 uart_bytes=452
[stats] quadros_ok=11 crc_falhos=0 bytes_descartados=1 acks_tx=11 acks_falhos=0 registros=11 duplicados=0 uart_bytes=452
[stats] quadros_ok=11 crc_falhos=0 bytes_descartados=1 acks_tx=11 acks_falhos=0 registros=11 duplicados=0 uart_bytes=452
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=3 tentativa=1
[rx] no=1 seq=3 tentativa=1 ts=1791240565
     RTC do S3: 2026-10-05 22:49:25 (sem sincronizacao externa)
     SHT30 interno: 28.32 C / 42.69 %RH
     SHT30 externo: 28.52 C / 44.61 %RH
     bateria medida=0.803 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=34.3 mg pico=259.2 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":3,"attempt":1,"ts":1791240565,"t_in":2832,"rh_in":4269,"t_out":2852,"rh_out":4461,"vbat_mv":803,"vib_rms":343,"vib_peak":2592,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=3 registro=3; conferir recepcao no S3
[stats] quadros_ok=12 crc_falhos=0 bytes_descartados=1 acks_tx=12 acks_falhos=0 registros=12 duplicados=0 uart_bytes=493
[stats] quadros_ok=12 crc_falhos=0 bytes_descartados=1 acks_tx=12 acks_falhos=0 registros=12 duplicados=0 uart_bytes=493
[stats] quadros_ok=12 crc_falhos=0 bytes_descartados=1 acks_tx=12 acks_falhos=0 registros=12 duplicados=0 uart_bytes=493
[stats] quadros_ok=12 crc_falhos=0 bytes_descartados=1 acks_tx=12 acks_falhos=0 registros=12 duplicados=0 uart_bytes=493
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=4 tentativa=1
[rx] no=1 seq=4 tentativa=1 ts=1791240625
     RTC do S3: 2026-10-05 22:50:25 (sem sincronizacao externa)
     SHT30 interno: 28.32 C / 42.85 %RH
     SHT30 externo: 28.53 C / 44.77 %RH
     bateria medida=0.812 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.5 mg pico=287.8 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":4,"attempt":1,"ts":1791240625,"t_in":2832,"rh_in":4285,"t_out":2853,"rh_out":4477,"vbat_mv":812,"vib_rms":335,"vib_peak":2878,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=4 registro=4; conferir recepcao no S3
[stats] quadros_ok=13 crc_falhos=0 bytes_descartados=1 acks_tx=13 acks_falhos=0 registros=13 duplicados=0 uart_bytes=534
[stats] quadros_ok=13 crc_falhos=0 bytes_descartados=1 acks_tx=13 acks_falhos=0 registros=13 duplicados=0 uart_bytes=534
[stats] quadros_ok=13 crc_falhos=0 bytes_descartados=1 acks_tx=13 acks_falhos=0 registros=13 duplicados=0 uart_bytes=534
[stats] quadros_ok=13 crc_falhos=0 bytes_descartados=1 acks_tx=13 acks_falhos=0 registros=13 duplicados=0 uart_bytes=534
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=5 tentativa=1
[rx] no=1 seq=5 tentativa=1 ts=1791240685
     RTC do S3: 2026-10-05 22:51:25 (sem sincronizacao externa)
     SHT30 interno: 28.34 C / 42.52 %RH
     SHT30 externo: 28.52 C / 44.41 %RH
     bateria medida=0.815 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.2 mg pico=268.2 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.00 22.00 23.50 25.50
BEEREC {"node":1,"seq":5,"attempt":1,"ts":1791240685,"t_in":2834,"rh_in":4252,"t_out":2852,"rh_out":4441,"vbat_mv":815,"vib_rms":332,"vib_peak":2682,"bands":[236,244,247,251],"flags":0}
[ACK] TX local OK: no=1 quadro=5 registro=5; conferir recepcao no S3
[stats] quadros_ok=14 crc_falhos=0 bytes_descartados=1 acks_tx=14 acks_falhos=0 registros=14 duplicados=0 uart_bytes=575
[stats] quadros_ok=14 crc_falhos=0 bytes_descartados=1 acks_tx=14 acks_falhos=0 registros=14 duplicados=0 uart_bytes=575
[stats] quadros_ok=14 crc_falhos=0 bytes_descartados=1 acks_tx=14 acks_falhos=0 registros=14 duplicados=0 uart_bytes=575
[stats] quadros_ok=14 crc_falhos=0 bytes_descartados=1 acks_tx=14 acks_falhos=0 registros=14 duplicados=0 uart_bytes=575
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=6 tentativa=1
[rx] no=1 seq=6 tentativa=1 ts=1791240745
     RTC do S3: 2026-10-05 22:52:25 (sem sincronizacao externa)
     SHT30 interno: 28.31 C / 42.66 %RH
     SHT30 externo: 28.48 C / 44.57 %RH
     bateria medida=0.808 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.6 mg pico=282.7 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.50 22.00 24.00 25.50
BEEREC {"node":1,"seq":6,"attempt":1,"ts":1791240745,"t_in":2831,"rh_in":4266,"t_out":2848,"rh_out":4457,"vbat_mv":808,"vib_rms":336,"vib_peak":2827,"bands":[239,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=6 registro=6; conferir recepcao no S3
[stats] quadros_ok=15 crc_falhos=0 bytes_descartados=1 acks_tx=15 acks_falhos=0 registros=15 duplicados=0 uart_bytes=616
[stats] quadros_ok=15 crc_falhos=0 bytes_descartados=1 acks_tx=15 acks_falhos=0 registros=15 duplicados=0 uart_bytes=616
[stats] quadros_ok=15 crc_falhos=0 bytes_descartados=1 acks_tx=15 acks_falhos=0 registros=15 duplicados=0 uart_bytes=616
[stats] quadros_ok=15 crc_falhos=0 bytes_descartados=1 acks_tx=15 acks_falhos=0 registros=15 duplicados=0 uart_bytes=616
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=7 tentativa=1
[rx] no=1 seq=7 tentativa=1 ts=1791240805
     RTC do S3: 2026-10-05 22:53:25 (sem sincronizacao externa)
     SHT30 interno: 28.23 C / 42.63 %RH
     SHT30 externo: 28.45 C / 44.53 %RH
     bateria medida=0.817 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.6 mg pico=266.8 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.00 22.00 23.50 25.50
BEEREC {"node":1,"seq":7,"attempt":1,"ts":1791240805,"t_in":2823,"rh_in":4263,"t_out":2845,"rh_out":4453,"vbat_mv":817,"vib_rms":336,"vib_peak":2668,"bands":[236,244,247,251],"flags":0}
[ACK] TX local OK: no=1 quadro=7 registro=7; conferir recepcao no S3
[stats] quadros_ok=16 crc_falhos=0 bytes_descartados=1 acks_tx=16 acks_falhos=0 registros=16 duplicados=0 uart_bytes=657
[stats] quadros_ok=16 crc_falhos=0 bytes_descartados=1 acks_tx=16 acks_falhos=0 registros=16 duplicados=0 uart_bytes=657
[stats] quadros_ok=16 crc_falhos=0 bytes_descartados=1 acks_tx=16 acks_falhos=0 registros=16 duplicados=0 uart_bytes=657
[stats] quadros_ok=16 crc_falhos=0 bytes_descartados=1 acks_tx=16 acks_falhos=0 registros=16 duplicados=0 uart_bytes=657
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=8 tentativa=1
[rx] no=1 seq=8 tentativa=1 ts=1791240865
     RTC do S3: 2026-10-05 22:54:25 (sem sincronizacao externa)
     SHT30 interno: 28.21 C / 42.33 %RH
     SHT30 externo: 28.42 C / 44.24 %RH
     bateria medida=0.810 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.4 mg pico=259.7 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.00 22.00 23.50 25.50
BEEREC {"node":1,"seq":8,"attempt":1,"ts":1791240865,"t_in":2821,"rh_in":4233,"t_out":2842,"rh_out":4424,"vbat_mv":810,"vib_rms":334,"vib_peak":2597,"bands":[236,244,247,251],"flags":0}
[ACK] TX local OK: no=1 quadro=8 registro=8; conferir recepcao no S3
[stats] quadros_ok=17 crc_falhos=0 bytes_descartados=1 acks_tx=17 acks_falhos=0 registros=17 duplicados=0 uart_bytes=698
[stats] quadros_ok=17 crc_falhos=0 bytes_descartados=1 acks_tx=17 acks_falhos=0 registros=17 duplicados=0 uart_bytes=698
[stats] quadros_ok=17 crc_falhos=0 bytes_descartados=1 acks_tx=17 acks_falhos=0 registros=17 duplicados=0 uart_bytes=698
[stats] quadros_ok=17 crc_falhos=0 bytes_descartados=1 acks_tx=17 acks_falhos=0 registros=17 duplicados=0 uart_bytes=698
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=9 tentativa=1
[rx] no=1 seq=9 tentativa=1 ts=1791240925
     RTC do S3: 2026-10-05 22:55:25 (sem sincronizacao externa)
     SHT30 interno: 28.15 C / 42.37 %RH
     SHT30 externo: 28.40 C / 44.29 %RH
     bateria medida=0.802 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.7 mg pico=281.8 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":9,"attempt":1,"ts":1791240925,"t_in":2815,"rh_in":4237,"t_out":2840,"rh_out":4429,"vbat_mv":802,"vib_rms":337,"vib_peak":2818,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=9 registro=9; conferir recepcao no S3
[stats] quadros_ok=18 crc_falhos=0 bytes_descartados=1 acks_tx=18 acks_falhos=0 registros=18 duplicados=0 uart_bytes=739
[stats] quadros_ok=18 crc_falhos=0 bytes_descartados=1 acks_tx=18 acks_falhos=0 registros=18 duplicados=0 uart_bytes=739
[stats] quadros_ok=18 crc_falhos=0 bytes_descartados=1 acks_tx=18 acks_falhos=0 registros=18 duplicados=0 uart_bytes=739
[stats] quadros_ok=18 crc_falhos=0 bytes_descartados=1 acks_tx=18 acks_falhos=0 registros=18 duplicados=0 uart_bytes=739
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=10 tentativa=1
[rx] no=1 seq=10 tentativa=1 ts=1791240985
     RTC do S3: 2026-10-05 22:56:25 (sem sincronizacao externa)
     SHT30 interno: 28.23 C / 42.08 %RH
     SHT30 externo: 28.41 C / 44.03 %RH
     bateria medida=0.799 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.8 mg pico=280.4 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.50 22.00 24.00 25.50
BEEREC {"node":1,"seq":10,"attempt":1,"ts":1791240985,"t_in":2823,"rh_in":4208,"t_out":2841,"rh_out":4403,"vbat_mv":799,"vib_rms":338,"vib_peak":2804,"bands":[237,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=10 registro=10; conferir recepcao no S3
[stats] quadros_ok=19 crc_falhos=0 bytes_descartados=1 acks_tx=19 acks_falhos=0 registros=19 duplicados=0 uart_bytes=780
[stats] quadros_ok=19 crc_falhos=0 bytes_descartados=1 acks_tx=19 acks_falhos=0 registros=19 duplicados=0 uart_bytes=780
[stats] quadros_ok=19 crc_falhos=0 bytes_descartados=1 acks_tx=19 acks_falhos=0 registros=19 duplicados=0 uart_bytes=780
[stats] quadros_ok=19 crc_falhos=0 bytes_descartados=1 acks_tx=19 acks_falhos=0 registros=19 duplicados=0 uart_bytes=780
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=11 tentativa=1
[rx] no=1 seq=11 tentativa=1 ts=1791241045
     RTC do S3: 2026-10-05 22:57:25 (sem sincronizacao externa)
     SHT30 interno: 28.27 C / 41.81 %RH
     SHT30 externo: 28.41 C / 43.79 %RH
     bateria medida=0.816 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.0 mg pico=274.1 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.50 22.00 23.50 25.50
BEEREC {"node":1,"seq":11,"attempt":1,"ts":1791241045,"t_in":2827,"rh_in":4181,"t_out":2841,"rh_out":4379,"vbat_mv":816,"vib_rms":330,"vib_peak":2741,"bands":[237,244,247,251],"flags":0}
[ACK] TX local OK: no=1 quadro=11 registro=11; conferir recepcao no S3
[stats] quadros_ok=20 crc_falhos=0 bytes_descartados=1 acks_tx=20 acks_falhos=0 registros=20 duplicados=0 uart_bytes=821
[stats] quadros_ok=20 crc_falhos=0 bytes_descartados=1 acks_tx=20 acks_falhos=0 registros=20 duplicados=0 uart_bytes=821
[stats] quadros_ok=20 crc_falhos=0 bytes_descartados=1 acks_tx=20 acks_falhos=0 registros=20 duplicados=0 uart_bytes=821
[stats] quadros_ok=20 crc_falhos=0 bytes_descartados=1 acks_tx=20 acks_falhos=0 registros=20 duplicados=0 uart_bytes=821
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=12 tentativa=1
[rx] no=1 seq=12 tentativa=1 ts=1791241105
     RTC do S3: 2026-10-05 22:58:25 (sem sincronizacao externa)
     SHT30 interno: 28.27 C / 41.71 %RH
     SHT30 externo: 28.44 C / 43.74 %RH
     bateria medida=0.806 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.6 mg pico=275.3 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.50 22.00 24.00 25.50
BEEREC {"node":1,"seq":12,"attempt":1,"ts":1791241105,"t_in":2827,"rh_in":4171,"t_out":2844,"rh_out":4374,"vbat_mv":806,"vib_rms":336,"vib_peak":2753,"bands":[237,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=12 registro=12; conferir recepcao no S3
[stats] quadros_ok=21 crc_falhos=0 bytes_descartados=1 acks_tx=21 acks_falhos=0 registros=21 duplicados=0 uart_bytes=862
[stats] quadros_ok=21 crc_falhos=0 bytes_descartados=1 acks_tx=21 acks_falhos=0 registros=21 duplicados=0 uart_bytes=862
[stats] quadros_ok=21 crc_falhos=0 bytes_descartados=1 acks_tx=21 acks_falhos=0 registros=21 duplicados=0 uart_bytes=862
[stats] quadros_ok=21 crc_falhos=0 bytes_descartados=1 acks_tx=21 acks_falhos=0 registros=21 duplicados=0 uart_bytes=862
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=13 tentativa=1
[rx] no=1 seq=13 tentativa=1 ts=1791241165
     RTC do S3: 2026-10-05 22:59:25 (sem sincronizacao externa)
     SHT30 interno: 28.27 C / 41.77 %RH
     SHT30 externo: 28.43 C / 43.77 %RH
     bateria medida=0.818 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.5 mg pico=252.0 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.50 22.00 24.00 25.50
BEEREC {"node":1,"seq":13,"attempt":1,"ts":1791241165,"t_in":2827,"rh_in":4177,"t_out":2843,"rh_out":4377,"vbat_mv":818,"vib_rms":335,"vib_peak":2520,"bands":[237,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=13 registro=13; conferir recepcao no S3
[stats] quadros_ok=22 crc_falhos=0 bytes_descartados=1 acks_tx=22 acks_falhos=0 registros=22 duplicados=0 uart_bytes=903
[stats] quadros_ok=22 crc_falhos=0 bytes_descartados=1 acks_tx=22 acks_falhos=0 registros=22 duplicados=0 uart_bytes=903
[stats] quadros_ok=22 crc_falhos=0 bytes_descartados=1 acks_tx=22 acks_falhos=0 registros=22 duplicados=0 uart_bytes=903
[stats] quadros_ok=22 crc_falhos=0 bytes_descartados=1 acks_tx=22 acks_falhos=0 registros=22 duplicados=0 uart_bytes=903
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=14 tentativa=1
[rx] no=1 seq=14 tentativa=1 ts=1791241225
     RTC do S3: 2026-10-05 23:00:25 (sem sincronizacao externa)
     SHT30 interno: 28.25 C / 41.93 %RH
     SHT30 externo: 28.44 C / 43.97 %RH
     bateria medida=0.802 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.5 mg pico=299.0 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":14,"attempt":1,"ts":1791241225,"t_in":2825,"rh_in":4193,"t_out":2844,"rh_out":4397,"vbat_mv":802,"vib_rms":335,"vib_peak":2990,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=14 registro=14; conferir recepcao no S3
[stats] quadros_ok=23 crc_falhos=0 bytes_descartados=1 acks_tx=23 acks_falhos=0 registros=23 duplicados=0 uart_bytes=944
[stats] quadros_ok=23 crc_falhos=0 bytes_descartados=1 acks_tx=23 acks_falhos=0 registros=23 duplicados=0 uart_bytes=944
[stats] quadros_ok=23 crc_falhos=0 bytes_descartados=1 acks_tx=23 acks_falhos=0 registros=23 duplicados=0 uart_bytes=944
[stats] quadros_ok=23 crc_falhos=0 bytes_descartados=1 acks_tx=23 acks_falhos=0 registros=23 duplicados=0 uart_bytes=944
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=15 tentativa=1
[rx] no=1 seq=15 tentativa=1 ts=1791241285
     RTC do S3: 2026-10-05 23:01:25 (sem sincronizacao externa)
     SHT30 interno: 28.27 C / 42.22 %RH
     SHT30 externo: 28.44 C / 44.25 %RH
     bateria medida=0.810 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.6 mg pico=268.9 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.50 22.00 24.00 25.50
BEEREC {"node":1,"seq":15,"attempt":1,"ts":1791241285,"t_in":2827,"rh_in":4222,"t_out":2844,"rh_out":4425,"vbat_mv":810,"vib_rms":336,"vib_peak":2689,"bands":[237,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=15 registro=15; conferir recepcao no S3
[stats] quadros_ok=24 crc_falhos=0 bytes_descartados=1 acks_tx=24 acks_falhos=0 registros=24 duplicados=0 uart_bytes=985
[stats] quadros_ok=24 crc_falhos=0 bytes_descartados=1 acks_tx=24 acks_falhos=0 registros=24 duplicados=0 uart_bytes=985
[stats] quadros_ok=24 crc_falhos=0 bytes_descartados=1 acks_tx=24 acks_falhos=0 registros=24 duplicados=0 uart_bytes=985
[stats] quadros_ok=24 crc_falhos=0 bytes_descartados=1 acks_tx=24 acks_falhos=0 registros=24 duplicados=0 uart_bytes=985
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=16 tentativa=1
[rx] no=1 seq=16 tentativa=1 ts=1791241345
     RTC do S3: 2026-10-05 23:02:25 (sem sincronizacao externa)
     SHT30 interno: 28.34 C / 42.11 %RH
     SHT30 externo: 28.45 C / 44.12 %RH
     bateria medida=0.816 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.2 mg pico=267.6 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.00 22.00 23.50 25.50
BEEREC {"node":1,"seq":16,"attempt":1,"ts":1791241345,"t_in":2834,"rh_in":4211,"t_out":2845,"rh_out":4412,"vbat_mv":816,"vib_rms":332,"vib_peak":2676,"bands":[236,244,247,251],"flags":0}
[ACK] TX local OK: no=1 quadro=16 registro=16; conferir recepcao no S3
[stats] quadros_ok=25 crc_falhos=0 bytes_descartados=1 acks_tx=25 acks_falhos=0 registros=25 duplicados=0 uart_bytes=1026
[stats] quadros_ok=25 crc_falhos=0 bytes_descartados=1 acks_tx=25 acks_falhos=0 registros=25 duplicados=0 uart_bytes=1026
[stats] quadros_ok=25 crc_falhos=0 bytes_descartados=1 acks_tx=25 acks_falhos=0 registros=25 duplicados=0 uart_bytes=1026
[stats] quadros_ok=25 crc_falhos=0 bytes_descartados=1 acks_tx=25 acks_falhos=0 registros=25 duplicados=0 uart_bytes=1026
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=17 tentativa=1
[rx] no=1 seq=17 tentativa=1 ts=1791241405
     RTC do S3: 2026-10-05 23:03:25 (sem sincronizacao externa)
     SHT30 interno: 28.29 C / 42.25 %RH
     SHT30 externo: 28.46 C / 44.26 %RH
     bateria medida=0.797 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.3 mg pico=305.4 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 19.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":17,"attempt":1,"ts":1791241405,"t_in":2829,"rh_in":4225,"t_out":2846,"rh_out":4426,"vbat_mv":797,"vib_rms":333,"vib_peak":3054,"bands":[238,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=17 registro=17; conferir recepcao no S3
[stats] quadros_ok=26 crc_falhos=0 bytes_descartados=1 acks_tx=26 acks_falhos=0 registros=26 duplicados=0 uart_bytes=1067
[stats] quadros_ok=26 crc_falhos=0 bytes_descartados=1 acks_tx=26 acks_falhos=0 registros=26 duplicados=0 uart_bytes=1067
[stats] quadros_ok=26 crc_falhos=0 bytes_descartados=1 acks_tx=26 acks_falhos=0 registros=26 duplicados=0 uart_bytes=1067
[stats] quadros_ok=26 crc_falhos=0 bytes_descartados=1 acks_tx=26 acks_falhos=0 registros=26 duplicados=0 uart_bytes=1067
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=18 tentativa=1
[rx] no=1 seq=18 tentativa=1 ts=1791241465
     RTC do S3: 2026-10-05 23:04:25 (sem sincronizacao externa)
     SHT30 interno: 28.23 C / 42.16 %RH
     SHT30 externo: 28.41 C / 44.19 %RH
     bateria medida=0.810 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.5 mg pico=259.3 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.50 22.00 24.00 25.50
BEEREC {"node":1,"seq":18,"attempt":1,"ts":1791241465,"t_in":2823,"rh_in":4216,"t_out":2841,"rh_out":4419,"vbat_mv":810,"vib_rms":335,"vib_peak":2593,"bands":[237,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=18 registro=18; conferir recepcao no S3
[stats] quadros_ok=27 crc_falhos=0 bytes_descartados=1 acks_tx=27 acks_falhos=0 registros=27 duplicados=0 uart_bytes=1108
[stats] quadros_ok=27 crc_falhos=0 bytes_descartados=1 acks_tx=27 acks_falhos=0 registros=27 duplicados=0 uart_bytes=1108
[stats] quadros_ok=27 crc_falhos=0 bytes_descartados=1 acks_tx=27 acks_falhos=0 registros=27 duplicados=0 uart_bytes=1108
[stats] quadros_ok=27 crc_falhos=0 bytes_descartados=1 acks_tx=27 acks_falhos=0 registros=27 duplicados=0 uart_bytes=1108
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=19 tentativa=1
[rx] no=1 seq=19 tentativa=1 ts=1791241525
     RTC do S3: 2026-10-05 23:05:25 (sem sincronizacao externa)
     SHT30 interno: 28.12 C / 42.13 %RH
     SHT30 externo: 28.35 C / 44.16 %RH
     bateria medida=0.789 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.3 mg pico=289.6 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 17.50 22.00 23.50 25.50
BEEREC {"node":1,"seq":19,"attempt":1,"ts":1791241525,"t_in":2812,"rh_in":4213,"t_out":2835,"rh_out":4416,"vbat_mv":789,"vib_rms":333,"vib_peak":2896,"bands":[235,244,247,251],"flags":0}
[ACK] TX local OK: no=1 quadro=19 registro=19; conferir recepcao no S3
[stats] quadros_ok=28 crc_falhos=0 bytes_descartados=1 acks_tx=28 acks_falhos=0 registros=28 duplicados=0 uart_bytes=1149
[stats] quadros_ok=28 crc_falhos=0 bytes_descartados=1 acks_tx=28 acks_falhos=0 registros=28 duplicados=0 uart_bytes=1149
[stats] quadros_ok=28 crc_falhos=0 bytes_descartados=1 acks_tx=28 acks_falhos=0 registros=28 duplicados=0 uart_bytes=1149
[stats] quadros_ok=28 crc_falhos=0 bytes_descartados=1 acks_tx=28 acks_falhos=0 registros=28 duplicados=0 uart_bytes=1149
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=20 tentativa=1
[rx] no=1 seq=20 tentativa=1 ts=1791241585
     RTC do S3: 2026-10-05 23:06:25 (sem sincronizacao externa)
     SHT30 interno: 28.12 C / 42.39 %RH
     SHT30 externo: 28.29 C / 44.39 %RH
     bateria medida=0.803 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=32.5 mg pico=283.0 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 15.50 21.50 23.00 26.00
BEEREC {"node":1,"seq":20,"attempt":1,"ts":1791241585,"t_in":2812,"rh_in":4239,"t_out":2829,"rh_out":4439,"vbat_mv":803,"vib_rms":325,"vib_peak":2830,"bands":[231,243,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=20 registro=20; conferir recepcao no S3
[stats] quadros_ok=29 crc_falhos=0 bytes_descartados=1 acks_tx=29 acks_falhos=0 registros=29 duplicados=0 uart_bytes=1190
[stats] quadros_ok=29 crc_falhos=0 bytes_descartados=1 acks_tx=29 acks_falhos=0 registros=29 duplicados=0 uart_bytes=1190
[stats] quadros_ok=29 crc_falhos=0 bytes_descartados=1 acks_tx=29 acks_falhos=0 registros=29 duplicados=0 uart_bytes=1190
[stats] quadros_ok=29 crc_falhos=0 bytes_descartados=1 acks_tx=29 acks_falhos=0 registros=29 duplicados=0 uart_bytes=1190
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=21 tentativa=1
[rx] no=1 seq=21 tentativa=1 ts=1791241645
     RTC do S3: 2026-10-05 23:07:25 (sem sincronizacao externa)
     SHT30 interno: 28.05 C / 42.53 %RH
     SHT30 externo: 28.25 C / 44.51 %RH
     bateria medida=0.804 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.2 mg pico=270.7 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.00 22.00 23.00 26.00
BEEREC {"node":1,"seq":21,"attempt":1,"ts":1791241645,"t_in":2805,"rh_in":4253,"t_out":2825,"rh_out":4451,"vbat_mv":804,"vib_rms":332,"vib_peak":2707,"bands":[232,244,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=21 registro=21; conferir recepcao no S3
[stats] quadros_ok=30 crc_falhos=0 bytes_descartados=1 acks_tx=30 acks_falhos=0 registros=30 duplicados=0 uart_bytes=1231
[stats] quadros_ok=30 crc_falhos=0 bytes_descartados=1 acks_tx=30 acks_falhos=0 registros=30 duplicados=0 uart_bytes=1231
[stats] quadros_ok=30 crc_falhos=0 bytes_descartados=1 acks_tx=30 acks_falhos=0 registros=30 duplicados=0 uart_bytes=1231
[stats] quadros_ok=30 crc_falhos=0 bytes_descartados=1 acks_tx=30 acks_falhos=0 registros=30 duplicados=0 uart_bytes=1231
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=22 tentativa=1
[rx] no=1 seq=22 tentativa=1 ts=1791241705
     RTC do S3: 2026-10-05 23:08:25 (sem sincronizacao externa)
     SHT30 interno: 28.02 C / 42.77 %RH
     SHT30 externo: 28.20 C / 44.69 %RH
     bateria medida=0.807 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=32.4 mg pico=252.2 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 15.50 21.50 23.00 26.00
BEEREC {"node":1,"seq":22,"attempt":1,"ts":1791241705,"t_in":2802,"rh_in":4277,"t_out":2820,"rh_out":4469,"vbat_mv":807,"vib_rms":324,"vib_peak":2522,"bands":[231,243,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=22 registro=22; conferir recepcao no S3
[stats] quadros_ok=31 crc_falhos=0 bytes_descartados=1 acks_tx=31 acks_falhos=0 registros=31 duplicados=0 uart_bytes=1272
[stats] quadros_ok=31 crc_falhos=0 bytes_descartados=1 acks_tx=31 acks_falhos=0 registros=31 duplicados=0 uart_bytes=1272
[stats] quadros_ok=31 crc_falhos=0 bytes_descartados=1 acks_tx=31 acks_falhos=0 registros=31 duplicados=0 uart_bytes=1272
[stats] quadros_ok=31 crc_falhos=0 bytes_descartados=1 acks_tx=31 acks_falhos=0 registros=31 duplicados=0 uart_bytes=1272
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=23 tentativa=1
[rx] no=1 seq=23 tentativa=1 ts=1791241765
     RTC do S3: 2026-10-05 23:09:25 (sem sincronizacao externa)
     SHT30 interno: 27.96 C / 42.89 %RH
     SHT30 externo: 28.16 C / 44.76 %RH
     bateria medida=0.808 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.3 mg pico=266.1 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.00 21.50 23.50 25.50
BEEREC {"node":1,"seq":23,"attempt":1,"ts":1791241765,"t_in":2796,"rh_in":4289,"t_out":2816,"rh_out":4476,"vbat_mv":808,"vib_rms":333,"vib_peak":2661,"bands":[236,243,247,251],"flags":0}
[ACK] TX local OK: no=1 quadro=23 registro=23; conferir recepcao no S3
[stats] quadros_ok=32 crc_falhos=0 bytes_descartados=1 acks_tx=32 acks_falhos=0 registros=32 duplicados=0 uart_bytes=1313
[stats] quadros_ok=32 crc_falhos=0 bytes_descartados=1 acks_tx=32 acks_falhos=0 registros=32 duplicados=0 uart_bytes=1313
[stats] quadros_ok=32 crc_falhos=0 bytes_descartados=1 acks_tx=32 acks_falhos=0 registros=32 duplicados=0 uart_bytes=1313
[stats] quadros_ok=32 crc_falhos=0 bytes_descartados=1 acks_tx=32 acks_falhos=0 registros=32 duplicados=0 uart_bytes=1313
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=24 tentativa=1
[rx] no=1 seq=24 tentativa=1 ts=1791241825
     RTC do S3: 2026-10-05 23:10:25 (sem sincronizacao externa)
     SHT30 interno: 28.01 C / 42.97 %RH
     SHT30 externo: 28.18 C / 44.80 %RH
     bateria medida=0.805 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.8 mg pico=274.1 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.00 22.00 24.00 25.50
BEEREC {"node":1,"seq":24,"attempt":1,"ts":1791241825,"t_in":2801,"rh_in":4297,"t_out":2818,"rh_out":4480,"vbat_mv":805,"vib_rms":338,"vib_peak":2741,"bands":[236,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=24 registro=24; conferir recepcao no S3
[stats] quadros_ok=33 crc_falhos=0 bytes_descartados=1 acks_tx=33 acks_falhos=0 registros=33 duplicados=0 uart_bytes=1354
[stats] quadros_ok=33 crc_falhos=0 bytes_descartados=1 acks_tx=33 acks_falhos=0 registros=33 duplicados=0 uart_bytes=1354
[stats] quadros_ok=33 crc_falhos=0 bytes_descartados=1 acks_tx=33 acks_falhos=0 registros=33 duplicados=0 uart_bytes=1354
[stats] quadros_ok=33 crc_falhos=0 bytes_descartados=1 acks_tx=33 acks_falhos=0 registros=33 duplicados=0 uart_bytes=1354
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=25 tentativa=1
[rx] no=1 seq=25 tentativa=1 ts=1791241885
     RTC do S3: 2026-10-05 23:11:25 (sem sincronizacao externa)
     SHT30 interno: 28.13 C / 43.20 %RH
     SHT30 externo: 28.29 C / 45.01 %RH
     bateria medida=0.810 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.6 mg pico=279.4 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.50 22.00 24.00 25.50
BEEREC {"node":1,"seq":25,"attempt":1,"ts":1791241885,"t_in":2813,"rh_in":4320,"t_out":2829,"rh_out":4501,"vbat_mv":810,"vib_rms":336,"vib_peak":2794,"bands":[237,244,248,251],"flags":0}
[ACK] TX local OK: no=1 quadro=25 registro=25; conferir recepcao no S3
[stats] quadros_ok=34 crc_falhos=0 bytes_descartados=1 acks_tx=34 acks_falhos=0 registros=34 duplicados=0 uart_bytes=1395
[stats] quadros_ok=34 crc_falhos=0 bytes_descartados=1 acks_tx=34 acks_falhos=0 registros=34 duplicados=0 uart_bytes=1395
[stats] quadros_ok=34 crc_falhos=0 bytes_descartados=1 acks_tx=34 acks_falhos=0 registros=34 duplicados=0 uart_bytes=1395
[stats] quadros_ok=34 crc_falhos=0 bytes_descartados=1 acks_tx=34 acks_falhos=0 registros=34 duplicados=0 uart_bytes=1395
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=26 tentativa=1
[rx] no=1 seq=26 tentativa=1 ts=1791241945
     RTC do S3: 2026-10-05 23:12:25 (sem sincronizacao externa)
     SHT30 interno: 28.12 C / 42.93 %RH
     SHT30 externo: 28.30 C / 44.86 %RH
     bateria medida=0.800 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=32.9 mg pico=276.4 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 15.50 22.00 23.00 26.00
BEEREC {"node":1,"seq":26,"attempt":1,"ts":1791241945,"t_in":2812,"rh_in":4293,"t_out":2830,"rh_out":4486,"vbat_mv":800,"vib_rms":329,"vib_peak":2764,"bands":[231,244,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=26 registro=26; conferir recepcao no S3
[stats] quadros_ok=35 crc_falhos=0 bytes_descartados=1 acks_tx=35 acks_falhos=0 registros=35 duplicados=0 uart_bytes=1436
[stats] quadros_ok=35 crc_falhos=0 bytes_descartados=1 acks_tx=35 acks_falhos=0 registros=35 duplicados=0 uart_bytes=1436
[stats] quadros_ok=35 crc_falhos=0 bytes_descartados=1 acks_tx=35 acks_falhos=0 registros=35 duplicados=0 uart_bytes=1436
[stats] quadros_ok=35 crc_falhos=0 bytes_descartados=1 acks_tx=35 acks_falhos=0 registros=35 duplicados=0 uart_bytes=1436
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=27 tentativa=1
[rx] no=1 seq=27 tentativa=1 ts=1791242005
     RTC do S3: 2026-10-05 23:13:25 (sem sincronizacao externa)
     SHT30 interno: 27.99 C / 42.77 %RH
     SHT30 externo: 28.21 C / 44.73 %RH
     bateria medida=0.809 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=32.9 mg pico=271.7 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 15.50 22.00 23.00 26.00
BEEREC {"node":1,"seq":27,"attempt":1,"ts":1791242005,"t_in":2799,"rh_in":4277,"t_out":2821,"rh_out":4473,"vbat_mv":809,"vib_rms":329,"vib_peak":2717,"bands":[231,244,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=27 registro=27; conferir recepcao no S3
[stats] quadros_ok=36 crc_falhos=0 bytes_descartados=1 acks_tx=36 acks_falhos=0 registros=36 duplicados=0 uart_bytes=1477
[stats] quadros_ok=36 crc_falhos=0 bytes_descartados=1 acks_tx=36 acks_falhos=0 registros=36 duplicados=0 uart_bytes=1477
[stats] quadros_ok=36 crc_falhos=0 bytes_descartados=1 acks_tx=36 acks_falhos=0 registros=36 duplicados=0 uart_bytes=1477
[stats] quadros_ok=36 crc_falhos=0 bytes_descartados=1 acks_tx=36 acks_falhos=0 registros=36 duplicados=0 uart_bytes=1477
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=28 tentativa=1
[rx] no=1 seq=28 tentativa=1 ts=1791242065
     RTC do S3: 2026-10-05 23:14:25 (sem sincronizacao externa)
     SHT30 interno: 27.98 C / 43.03 %RH
     SHT30 externo: 28.18 C / 44.95 %RH
     bateria medida=0.805 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.1 mg pico=267.4 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 18.00 22.00 23.50 25.50
BEEREC {"node":1,"seq":28,"attempt":1,"ts":1791242065,"t_in":2798,"rh_in":4303,"t_out":2818,"rh_out":4495,"vbat_mv":805,"vib_rms":331,"vib_peak":2674,"bands":[236,244,247,251],"flags":0}
[ACK] TX local OK: no=1 quadro=28 registro=28; conferir recepcao no S3
[stats] quadros_ok=37 crc_falhos=0 bytes_descartados=1 acks_tx=37 acks_falhos=0 registros=37 duplicados=0 uart_bytes=1518
[stats] quadros_ok=37 crc_falhos=0 bytes_descartados=1 acks_tx=37 acks_falhos=0 registros=37 duplicados=0 uart_bytes=1518
[stats] quadros_ok=37 crc_falhos=0 bytes_descartados=1 acks_tx=37 acks_falhos=0 registros=37 duplicados=0 uart_bytes=1518
[stats] quadros_ok=37 crc_falhos=0 bytes_descartados=1 acks_tx=37 acks_falhos=0 registros=37 duplicados=0 uart_bytes=1518
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=29 tentativa=1
[rx] no=1 seq=29 tentativa=1 ts=1791242125
     RTC do S3: 2026-10-05 23:15:25 (sem sincronizacao externa)
     SHT30 interno: 27.97 C / 43.01 %RH
     SHT30 externo: 28.15 C / 44.92 %RH
     bateria medida=0.818 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.6 mg pico=283.7 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.00 22.00 23.00 26.00
BEEREC {"node":1,"seq":29,"attempt":1,"ts":1791242125,"t_in":2797,"rh_in":4301,"t_out":2815,"rh_out":4492,"vbat_mv":818,"vib_rms":336,"vib_peak":2837,"bands":[232,244,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=29 registro=29; conferir recepcao no S3
[stats] quadros_ok=38 crc_falhos=0 bytes_descartados=1 acks_tx=38 acks_falhos=0 registros=38 duplicados=0 uart_bytes=1559
[stats] quadros_ok=38 crc_falhos=0 bytes_descartados=1 acks_tx=38 acks_falhos=0 registros=38 duplicados=0 uart_bytes=1559
[stats] quadros_ok=38 crc_falhos=0 bytes_descartados=1 acks_tx=38 acks_falhos=0 registros=38 duplicados=0 uart_bytes=1559
[stats] quadros_ok=38 crc_falhos=0 bytes_descartados=1 acks_tx=38 acks_falhos=0 registros=38 duplicados=0 uart_bytes=1559
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=30 tentativa=1
[rx] no=1 seq=30 tentativa=1 ts=1791242185
     RTC do S3: 2026-10-05 23:16:25 (sem sincronizacao externa)
     SHT30 interno: 27.82 C / 43.01 %RH
     SHT30 externo: 28.06 C / 44.95 %RH
     bateria medida=0.807 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.6 mg pico=288.6 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.00 22.00 23.50 26.00
BEEREC {"node":1,"seq":30,"attempt":1,"ts":1791242185,"t_in":2782,"rh_in":4301,"t_out":2806,"rh_out":4495,"vbat_mv":807,"vib_rms":336,"vib_peak":2886,"bands":[232,244,247,252],"flags":0}
[ACK] TX local OK: no=1 quadro=30 registro=30; conferir recepcao no S3
[stats] quadros_ok=39 crc_falhos=0 bytes_descartados=1 acks_tx=39 acks_falhos=0 registros=39 duplicados=0 uart_bytes=1600
[stats] quadros_ok=39 crc_falhos=0 bytes_descartados=1 acks_tx=39 acks_falhos=0 registros=39 duplicados=0 uart_bytes=1600
[stats] quadros_ok=39 crc_falhos=0 bytes_descartados=1 acks_tx=39 acks_falhos=0 registros=39 duplicados=0 uart_bytes=1600
[stats] quadros_ok=39 crc_falhos=0 bytes_descartados=1 acks_tx=39 acks_falhos=0 registros=39 duplicados=0 uart_bytes=1600
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=31 tentativa=1
[rx] no=1 seq=31 tentativa=1 ts=1791242245
     RTC do S3: 2026-10-05 23:17:25 (sem sincronizacao externa)
     SHT30 interno: 27.82 C / 43.19 %RH
     SHT30 externo: 28.03 C / 45.07 %RH
     bateria medida=0.804 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=32.3 mg pico=255.9 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.00 21.50 23.00 26.00
BEEREC {"node":1,"seq":31,"attempt":1,"ts":1791242245,"t_in":2782,"rh_in":4319,"t_out":2803,"rh_out":4507,"vbat_mv":804,"vib_rms":323,"vib_peak":2559,"bands":[232,243,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=31 registro=31; conferir recepcao no S3
[stats] quadros_ok=40 crc_falhos=0 bytes_descartados=1 acks_tx=40 acks_falhos=0 registros=40 duplicados=0 uart_bytes=1641
[stats] quadros_ok=40 crc_falhos=0 bytes_descartados=1 acks_tx=40 acks_falhos=0 registros=40 duplicados=0 uart_bytes=1641
[stats] quadros_ok=40 crc_falhos=0 bytes_descartados=1 acks_tx=40 acks_falhos=0 registros=40 duplicados=0 uart_bytes=1641
[stats] quadros_ok=40 crc_falhos=0 bytes_descartados=1 acks_tx=40 acks_falhos=0 registros=40 duplicados=0 uart_bytes=1641
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=32 tentativa=1
[rx] no=1 seq=32 tentativa=1 ts=1791242305
     RTC do S3: 2026-10-05 23:18:25 (sem sincronizacao externa)
     SHT30 interno: 27.79 C / 43.41 %RH
     SHT30 externo: 27.98 C / 45.22 %RH
     bateria medida=0.805 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=32.5 mg pico=274.4 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.00 22.00 23.00 26.00
BEEREC {"node":1,"seq":32,"attempt":1,"ts":1791242305,"t_in":2779,"rh_in":4341,"t_out":2798,"rh_out":4522,"vbat_mv":805,"vib_rms":325,"vib_peak":2744,"bands":[232,244,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=32 registro=32; conferir recepcao no S3
[stats] quadros_ok=41 crc_falhos=0 bytes_descartados=1 acks_tx=41 acks_falhos=0 registros=41 duplicados=0 uart_bytes=1682
[stats] quadros_ok=41 crc_falhos=0 bytes_descartados=1 acks_tx=41 acks_falhos=0 registros=41 duplicados=0 uart_bytes=1682
[stats] quadros_ok=41 crc_falhos=0 bytes_descartados=1 acks_tx=41 acks_falhos=0 registros=41 duplicados=0 uart_bytes=1682
[stats] quadros_ok=41 crc_falhos=0 bytes_descartados=1 acks_tx=41 acks_falhos=0 registros=41 duplicados=0 uart_bytes=1682
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=33 tentativa=1
[rx] no=1 seq=33 tentativa=1 ts=1791242365
     RTC do S3: 2026-10-05 23:19:25 (sem sincronizacao externa)
     SHT30 interno: 27.70 C / 43.38 %RH
     SHT30 externo: 27.95 C / 45.19 %RH
     bateria medida=0.811 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=32.7 mg pico=306.4 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.00 22.00 23.00 26.00
BEEREC {"node":1,"seq":33,"attempt":1,"ts":1791242365,"t_in":2770,"rh_in":4338,"t_out":2795,"rh_out":4519,"vbat_mv":811,"vib_rms":327,"vib_peak":3064,"bands":[232,244,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=33 registro=33; conferir recepcao no S3
[stats] quadros_ok=42 crc_falhos=0 bytes_descartados=1 acks_tx=42 acks_falhos=0 registros=42 duplicados=0 uart_bytes=1723
[stats] quadros_ok=42 crc_falhos=0 bytes_descartados=1 acks_tx=42 acks_falhos=0 registros=42 duplicados=0 uart_bytes=1723
[stats] quadros_ok=42 crc_falhos=0 bytes_descartados=1 acks_tx=42 acks_falhos=0 registros=42 duplicados=0 uart_bytes=1723
[stats] quadros_ok=42 crc_falhos=0 bytes_descartados=1 acks_tx=42 acks_falhos=0 registros=42 duplicados=0 uart_bytes=1723
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=34 tentativa=1
[rx] no=1 seq=34 tentativa=1 ts=1791242425
     RTC do S3: 2026-10-05 23:20:25 (sem sincronizacao externa)
     SHT30 interno: 27.65 C / 43.37 %RH
     SHT30 externo: 27.91 C / 45.18 %RH
     bateria medida=0.794 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.5 mg pico=266.7 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.50 22.00 23.50 26.00
BEEREC {"node":1,"seq":34,"attempt":1,"ts":1791242425,"t_in":2765,"rh_in":4337,"t_out":2791,"rh_out":4518,"vbat_mv":794,"vib_rms":335,"vib_peak":2667,"bands":[233,244,247,252],"flags":0}
[ACK] TX local OK: no=1 quadro=34 registro=34; conferir recepcao no S3
[stats] quadros_ok=43 crc_falhos=0 bytes_descartados=1 acks_tx=43 acks_falhos=0 registros=43 duplicados=0 uart_bytes=1764
[stats] quadros_ok=43 crc_falhos=0 bytes_descartados=1 acks_tx=43 acks_falhos=0 registros=43 duplicados=0 uart_bytes=1764
[stats] quadros_ok=43 crc_falhos=0 bytes_descartados=1 acks_tx=43 acks_falhos=0 registros=43 duplicados=0 uart_bytes=1764
[stats] quadros_ok=43 crc_falhos=0 bytes_descartados=1 acks_tx=43 acks_falhos=0 registros=43 duplicados=0 uart_bytes=1764
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=35 tentativa=1
[rx] no=1 seq=35 tentativa=1 ts=1791242485
     RTC do S3: 2026-10-05 23:21:25 (sem sincronizacao externa)
     SHT30 interno: 27.58 C / 43.40 %RH
     SHT30 externo: 27.87 C / 45.19 %RH
     bateria medida=0.804 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.0 mg pico=282.5 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.50 22.00 23.50 26.00
BEEREC {"node":1,"seq":35,"attempt":1,"ts":1791242485,"t_in":2758,"rh_in":4340,"t_out":2787,"rh_out":4519,"vbat_mv":804,"vib_rms":330,"vib_peak":2825,"bands":[233,244,247,252],"flags":0}
[ACK] TX local OK: no=1 quadro=35 registro=35; conferir recepcao no S3
[stats] quadros_ok=44 crc_falhos=0 bytes_descartados=1 acks_tx=44 acks_falhos=0 registros=44 duplicados=0 uart_bytes=1805
[stats] quadros_ok=44 crc_falhos=0 bytes_descartados=1 acks_tx=44 acks_falhos=0 registros=44 duplicados=0 uart_bytes=1805
[stats] quadros_ok=44 crc_falhos=0 bytes_descartados=1 acks_tx=44 acks_falhos=0 registros=44 duplicados=0 uart_bytes=1805
[stats] quadros_ok=44 crc_falhos=0 bytes_descartados=1 acks_tx=44 acks_falhos=0 registros=44 duplicados=0 uart_bytes=1805
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=36 tentativa=1
[rx] no=1 seq=36 tentativa=1 ts=1791242545
     RTC do S3: 2026-10-05 23:22:25 (sem sincronizacao externa)
     SHT30 interno: 27.55 C / 43.35 %RH
     SHT30 externo: 27.85 C / 45.12 %RH
     bateria medida=0.805 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.4 mg pico=252.7 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 17.50 22.00 23.50 26.00
BEEREC {"node":1,"seq":36,"attempt":1,"ts":1791242545,"t_in":2755,"rh_in":4335,"t_out":2785,"rh_out":4512,"vbat_mv":805,"vib_rms":334,"vib_peak":2527,"bands":[235,244,247,252],"flags":0}
[ACK] TX local OK: no=1 quadro=36 registro=36; conferir recepcao no S3
[stats] quadros_ok=45 crc_falhos=0 bytes_descartados=1 acks_tx=45 acks_falhos=0 registros=45 duplicados=0 uart_bytes=1846
[stats] quadros_ok=45 crc_falhos=0 bytes_descartados=1 acks_tx=45 acks_falhos=0 registros=45 duplicados=0 uart_bytes=1846
[stats] quadros_ok=45 crc_falhos=0 bytes_descartados=1 acks_tx=45 acks_falhos=0 registros=45 duplicados=0 uart_bytes=1846
[stats] quadros_ok=45 crc_falhos=0 bytes_descartados=1 acks_tx=45 acks_falhos=0 registros=45 duplicados=0 uart_bytes=1846
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=37 tentativa=1
[rx] no=1 seq=37 tentativa=1 ts=1791242605
     RTC do S3: 2026-10-05 23:23:25 (sem sincronizacao externa)
     SHT30 interno: 27.49 C / 43.25 %RH
     SHT30 externo: 27.78 C / 45.00 %RH
     bateria medida=0.802 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=32.6 mg pico=274.8 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.50 22.00 23.00 25.50
BEEREC {"node":1,"seq":37,"attempt":1,"ts":1791242605,"t_in":2749,"rh_in":4325,"t_out":2778,"rh_out":4500,"vbat_mv":802,"vib_rms":326,"vib_peak":2748,"bands":[233,244,246,251],"flags":0}
[ACK] TX local OK: no=1 quadro=37 registro=37; conferir recepcao no S3
[stats] quadros_ok=46 crc_falhos=0 bytes_descartados=1 acks_tx=46 acks_falhos=0 registros=46 duplicados=0 uart_bytes=1887
[stats] quadros_ok=46 crc_falhos=0 bytes_descartados=1 acks_tx=46 acks_falhos=0 registros=46 duplicados=0 uart_bytes=1887
[stats] quadros_ok=46 crc_falhos=0 bytes_descartados=1 acks_tx=46 acks_falhos=0 registros=46 duplicados=0 uart_bytes=1887
[stats] quadros_ok=46 crc_falhos=0 bytes_descartados=1 acks_tx=46 acks_falhos=0 registros=46 duplicados=0 uart_bytes=1887
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=38 tentativa=1
[rx] no=1 seq=38 tentativa=1 ts=1791242665
     RTC do S3: 2026-10-05 23:24:25 (sem sincronizacao externa)
     SHT30 interno: 27.52 C / 43.37 %RH
     SHT30 externo: 27.80 C / 45.11 %RH
     bateria medida=0.804 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=34.1 mg pico=289.0 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 16.00 22.00 23.50 26.00
BEEREC {"node":1,"seq":38,"attempt":1,"ts":1791242665,"t_in":2752,"rh_in":4337,"t_out":2780,"rh_out":4511,"vbat_mv":804,"vib_rms":341,"vib_peak":2890,"bands":[232,244,247,252],"flags":0}
[ACK] TX local OK: no=1 quadro=38 registro=38; conferir recepcao no S3
[stats] quadros_ok=47 crc_falhos=0 bytes_descartados=1 acks_tx=47 acks_falhos=0 registros=47 duplicados=0 uart_bytes=1928
[stats] quadros_ok=47 crc_falhos=0 bytes_descartados=1 acks_tx=47 acks_falhos=0 registros=47 duplicados=0 uart_bytes=1928
[stats] quadros_ok=47 crc_falhos=0 bytes_descartados=1 acks_tx=47 acks_falhos=0 registros=47 duplicados=0 uart_bytes=1928
[stats] quadros_ok=47 crc_falhos=0 bytes_descartados=1 acks_tx=47 acks_falhos=0 registros=47 duplicados=0 uart_bytes=1928
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=39 tentativa=1
[rx] no=1 seq=39 tentativa=1 ts=1791242725
     RTC do S3: 2026-10-05 23:25:25 (sem sincronizacao externa)
     SHT30 interno: 27.53 C / 43.59 %RH
     SHT30 externo: 27.80 C / 45.32 %RH
     bateria medida=0.812 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.3 mg pico=253.5 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 15.50 22.00 23.00 26.00
BEEREC {"node":1,"seq":39,"attempt":1,"ts":1791242725,"t_in":2753,"rh_in":4359,"t_out":2780,"rh_out":4532,"vbat_mv":812,"vib_rms":333,"vib_peak":2535,"bands":[231,244,246,252],"flags":0}
[ACK] TX local OK: no=1 quadro=39 registro=39; conferir recepcao no S3
[stats] quadros_ok=48 crc_falhos=0 bytes_descartados=1 acks_tx=48 acks_falhos=0 registros=48 duplicados=0 uart_bytes=1969
[stats] quadros_ok=48 crc_falhos=0 bytes_descartados=1 acks_tx=48 acks_falhos=0 registros=48 duplicados=0 uart_bytes=1969
[stats] quadros_ok=48 crc_falhos=0 bytes_descartados=1 acks_tx=48 acks_falhos=0 registros=48 duplicados=0 uart_bytes=1969
[stats] quadros_ok=48 crc_falhos=0 bytes_descartados=1 acks_tx=48 acks_falhos=0 registros=48 duplicados=0 uart_bytes=1969
[LoRa RX] OK: quadro validado com CRC32 | no=1 boot=234 quadro=40 tentativa=1
[rx] no=1 seq=40 tentativa=1 ts=1791242785
     RTC do S3: 2026-10-05 23:26:25 (sem sincronizacao externa)
     SHT30 interno: 27.56 C / 43.60 %RH
     SHT30 externo: 27.82 C / 45.33 %RH
     bateria medida=0.810 V | SD do S3=OK | flags=0x0000
     vibracao: RMS=33.6 mg pico=281.5 mg | ADXL=OK FIFO=OK
     bandas dB (50-100 / 100-200 / 200-350 / 350-600 Hz): 17.00 22.00 23.50 26.00
BEEREC {"node":1,"seq":40,"attempt":1,"ts":1791242785,"t_in":2756,"rh_in":4360,"t_out":2782,"rh_out":4533,"vbat_mv":810,"vib_rms":336,"vib_peak":2815,"bands":[234,244,247,252],"flags":0}
[ACK] TX local OK: no=1 quadro=40 registro=40; conferir recepcao no S3
[stats] quadros_ok=49 crc_falhos=0 bytes_descartados=1 acks_tx=49 acks_falhos=0 registros=49 duplicados=0 uart_bytes=2010
[stats] quadros_ok=49 crc_falhos=0 bytes_descartados=1 acks_tx=49 acks_falhos=0 registros=49 duplicados=0 uart_bytes=2010
[stats] quadros_ok=49 crc_falhos=0 bytes_descartados=1 acks_tx=49 acks_falhos=0 registros=49 duplicados=0 uart_bytes=2010