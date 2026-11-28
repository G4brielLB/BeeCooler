import struct
from decimal import Decimal

# Registro canônico v1: 24 bytes, little-endian (Seção 5.2 / 7.1)
# Formato: < I h H h H H H H 4B H
# - I: ts (uint32)
# - h: t_in (int16)
# - H: rh_in (uint16)
# - h: t_out (int16)
# - H: rh_out (uint16)
# - H: vbat (uint16)
# - H: vib_rms (uint16)
# - H: vib_peak (uint16)
# - 4B: b0, b1, b2, b3 (uint8 x 4)
# - H: flags (uint16)
REC_V1 = struct.Struct('<IhHhHHHH4BH')
assert REC_V1.size == 24

# Valores de sentinela para indicar ausência/falha de leitura
I16_NA = -32768
U16_NA = 0xFFFF
U8_NA = 0xFF

def _v(x, na, k):
    """Converte o valor aplicando o fator k ou retorna None se for sentinela."""
    if x == na:
        return None
    # Converte para Decimal para compatibilidade nativa com o DynamoDB
    return Decimal(str(round(x * k, 4)))

def decode_v1(buf: bytes, seq: int) -> dict:
    """
    Descodifica um buffer de 24 bytes do registro canônico v1.
    """
    if len(buf) != 24:
        raise ValueError(f"Tamanho de buffer inválido: esperado 24 bytes, recebido {len(buf)}")

    ts, t_in, rh_in, t_out, rh_out, vbat, rms, peak, b0, b1, b2, b3, flags = REC_V1.unpack(buf)

    # Extrai o power_level dos bits 9 e 10 das flags
    power_level = (flags >> 9) & 0b11

    # Descodificação das bandas de vibração (dB)
    band_db = []
    for b in (b0, b1, b2, b3):
        if b == U8_NA:
            band_db.append(None)
        else:
            band_db.append(Decimal(str(b / 2.0 - 100.0)))

    return {
        'seq': seq,
        'ts': ts,
        'ts_raw': ts,
        'flags': flags,
        'power_level': power_level,
        't_in': _v(t_in, I16_NA, 0.01),       # Ex: 2550 -> 25.50 °C
        'rh_in': _v(rh_in, U16_NA, 0.01),     # Ex: 6000 -> 60.00 %
        't_out': _v(t_out, I16_NA, 0.01),    # Ex: 3000 -> 30.00 °C
        'rh_out': _v(rh_out, U16_NA, 0.01),   # Ex: 5500 -> 55.00 %
        'vbat': _v(vbat, U16_NA, 0.001),      # Ex: 3700 -> 3.700 V
        'vib_rms': _v(rms, U16_NA, 0.1),      # em mg
        'vib_peak': _v(peak, U16_NA, 0.1),    # em mg
        'band_db': band_db,
        'raw': buf,
    }

# STATUS: 36 bytes = registro v1 mais recente (24 B) + saúde do nó (12 B).
# Layout de BeeCoolerLink::encodeStatus (v1.2, seção 6.3), little-endian.
STATUS_TAIL = struct.Struct('<HHBBBBHH')
STATUS_SIZE = REC_V1.size + STATUS_TAIL.size
assert STATUS_SIZE == 36

# esp_reset_reason_t (ESP-IDF)
RESET_REASONS = ['desconhecido', 'energia ligada', 'pino EN', 'software', 'pânico',
                 'watchdog de interrupção', 'watchdog de tarefa', 'watchdog', 'deep sleep',
                 'brownout', 'SDIO', 'USB', 'JTAG', 'eFuse', 'glitch de energia', 'CPU travada']

def fw_text(v: int) -> str:
    """0x0100 -> '1.0' (byte alto = major, byte baixo = minor)."""
    return f"{v >> 8}.{v & 0xFF}"

def decode_status(buf: bytes) -> dict:
    """Descodifica o STATUS de 36 bytes. O seq do registro embutido não vem no payload."""
    if len(buf) != STATUS_SIZE:
        raise ValueError(f"Tamanho de STATUS inválido: esperado {STATUS_SIZE} bytes, recebido {len(buf)}")
    rec = decode_v1(buf[:24], seq=None)
    backlog, sd_free_mb, sd_errors, reset, rssi, failed, vbat_mv, fw = STATUS_TAIL.unpack(buf[24:])
    return {
        'record': rec,
        'backlog': backlog,
        'sd_free_mb': None if sd_free_mb == U16_NA else sd_free_mb,
        'sd_errors': sd_errors,
        'reset_reason': RESET_REASONS[reset] if reset < len(RESET_REASONS) else f'código {reset}',
        'rssi_ack': None if rssi in (0, U8_NA) else -rssi,   # vem como -dBm; 0 = não medido
        'failed_sessions': failed,
        'vbat_load': _v(vbat_mv, U16_NA, 0.001),
        'fw_version': fw_text(fw),
    }
