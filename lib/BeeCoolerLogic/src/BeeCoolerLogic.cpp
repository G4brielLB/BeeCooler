#include "BeeCoolerLogic.h"

#include <string.h>

namespace BeeCoolerLogic {

namespace {

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (m == 2 && isLeap(y)) ? 29 : kDays[m - 1];
}

// Days since 1970-01-01 (Howard Hinnant's algorithm).
int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

}  // namespace

bool civilToEpoch(int year, int month, int day, int hour, int minute,
                  int second, uint32_t& epoch) {
  if (year < 1970 || year > 2105 || month < 1 || month > 12 || day < 1 ||
      day > daysInMonth(year, month) || hour < 0 || hour > 23 || minute < 0 ||
      minute > 59 || second < 0 || second > 59) {
    return false;
  }
  const int64_t days = daysFromCivil(year, month, day);
  epoch = static_cast<uint32_t>(days * 86400 + hour * 3600 + minute * 60 +
                                second);
  return true;
}

void epochToCivil(uint32_t epoch, int& year, int& month, int& day, int& hour,
                  int& minute, int& second) {
  int64_t days = epoch / 86400U;
  const uint32_t rem = epoch % 86400U;
  hour = static_cast<int>(rem / 3600U);
  minute = static_cast<int>((rem % 3600U) / 60U);
  second = static_cast<int>(rem % 60U);
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const int64_t doe = days - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = yoe + era * 400;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  year = static_cast<int>(y + (month <= 2));
}

bool parseBuildEpoch(const char* date, const char* time, uint32_t& epoch) {
  static const char* kMonths = "JanFebMarAprMayJunJulAugSepOctNovDec";
  if (date == nullptr || time == nullptr || strlen(date) < 11U ||
      strlen(time) < 8U) {
    return false;
  }
  int month = 0;
  for (int i = 0; i < 12; ++i) {
    if (strncmp(date, kMonths + 3 * i, 3) == 0) month = i + 1;
  }
  const int day = (date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0');
  const int year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 +
                   (date[9] - '0') * 10 + (date[10] - '0');
  const int hour = (time[0] - '0') * 10 + (time[1] - '0');
  const int minute = (time[3] - '0') * 10 + (time[4] - '0');
  const int second = (time[6] - '0') * 10 + (time[7] - '0');
  return month != 0 && civilToEpoch(year, month, day, hour, minute, second,
                                    epoch);
}

uint32_t nearestSlot(uint32_t now, uint32_t period_s) {
  if (period_s == 0U) return now;
  return ((now + period_s / 2U) / period_s) * period_s;
}

uint32_t sleepSeconds(uint32_t now, uint32_t period_s, uint32_t margin_s) {
  if (period_s == 0U) return 1U;
  const uint32_t next = (now / period_s + 1U) * period_s;
  const uint32_t until = next - now;
  return until > margin_s ? until - margin_s : 1U;
}

bool isLoraSlot(uint32_t slot, uint32_t lora_period_s) {
  return lora_period_s != 0U && slot % lora_period_s == 0U;
}

uint32_t loraOffsetSeconds(uint8_t node_id) {
  const uint32_t index = node_id > 0U ? node_id - 1U : 0U;
  return 20U + index * 30U;
}

LevelParams levelParams(uint8_t level, uint32_t divisor) {
  if (divisor == 0U) divisor = 1U;
  LevelParams p;
  switch (level) {
    case 0:
      p = {600U, true, true, true, 3600U, false};
      break;
    case 1:  // economy: no vibration, LoRa every 4 h
      p = {600U, true, false, true, 14400U, false};
      break;
    case 2:  // critical: T/RH every 30 min, one STATUS per day
      p = {1800U, true, false, true, 86400U, true};
      break;
    default:  // hibernation: wake every 2 h only to measure the voltage
      p = {7200U, false, false, false, 0U, true};
      break;
  }
  p.cycle_period_s /= divisor;
  p.lora_period_s /= divisor;
  return p;
}

bool powerUpdate(PowerState& s, uint16_t vbat_mv,
                 const PowerThresholds& t) {
  uint8_t candidate = s.level;
  // Deepest level whose entry threshold has been crossed.
  uint8_t deeper = 0U;
  for (uint8_t i = 0U; i < 3U; ++i) {
    if (vbat_mv < t.enter_mv[i]) deeper = static_cast<uint8_t>(i + 1U);
  }
  if (deeper > s.level) {
    candidate = deeper;
  } else {
    uint8_t c = s.level;
    while (c > 0U && vbat_mv > t.exit_mv[c - 1U]) --c;
    candidate = c;
  }

  if (candidate == s.level) {
    s.streak = 0U;
    s.pending_level = s.level;
    return false;
  }
  if (candidate == s.pending_level && s.streak > 0U) {
    ++s.streak;
  } else {
    s.pending_level = candidate;
    s.streak = 1U;
  }
  if (s.streak >= kLevelChangeReadings) {
    s.level = s.pending_level;
    s.streak = 0U;
    return true;
  }
  return false;
}

uint16_t median3(uint16_t a, uint16_t b, uint16_t c) {
  if ((a >= b && a <= c) || (a <= b && a >= c)) return a;
  if ((b >= a && b <= c) || (b <= a && b >= c)) return b;
  return c;
}

uint8_t shtCrc8(const uint8_t* data, size_t length) {
  uint8_t crc = 0xFFU;
  for (size_t i = 0U; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc & 0x80U) ? static_cast<uint8_t>((crc << 1) ^ 0x31U)
                          : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

bool shtDecode(const uint8_t* b, int16_t& t_centi, uint16_t& rh_centi) {
  if (shtCrc8(b, 2U) != b[2] || shtCrc8(b + 3, 2U) != b[5]) return false;
  const uint32_t raw_t = (static_cast<uint32_t>(b[0]) << 8) | b[1];
  const uint32_t raw_rh = (static_cast<uint32_t>(b[3]) << 8) | b[4];
  // T = -45 + 175 * raw / 65535 ; RH = 100 * raw / 65535 (0.01 units)
  const int32_t t = static_cast<int32_t>((17500LL * raw_t + 32767) / 65535) -
                    4500;
  const int32_t rh = static_cast<int32_t>((10000LL * raw_rh + 32767) / 65535);
  if (t < -4000 || t > 12500 || rh < 0 || rh > 10000) return false;
  t_centi = static_cast<int16_t>(t);
  rh_centi = static_cast<uint16_t>(rh);
  return true;
}

bool medianValid(const int16_t* values, const bool* valid, size_t count,
                 int16_t& out) {
  int16_t v[8];
  size_t n = 0U;
  for (size_t i = 0U; i < count && n < 8U; ++i) {
    if (valid[i]) v[n++] = values[i];
  }
  if (n == 0U) return false;
  for (size_t i = 1U; i < n; ++i) {  // insertion sort
    const int16_t key = v[i];
    size_t j = i;
    while (j > 0U && v[j - 1U] > key) {
      v[j] = v[j - 1U];
      --j;
    }
    v[j] = key;
  }
  out = (n % 2U == 1U) ? v[n / 2U]
                       : static_cast<int16_t>((v[n / 2U - 1U] + v[n / 2U]) / 2);
  return true;
}

void stuckPush(StuckHistory& h, int16_t t_in, uint16_t rh_in, int16_t t_out) {
  h.t_in[h.head] = t_in;
  h.rh_in[h.head] = rh_in;
  h.t_out[h.head] = t_out;
  h.head = static_cast<uint8_t>((h.head + 1U) % StuckHistory::kLength);
  if (h.count < StuckHistory::kLength) ++h.count;
}

bool stuckDetected(const StuckHistory& h) {
  if (h.count < StuckHistory::kLength) return false;
  int32_t tin_min = INT32_MAX, tin_max = INT32_MIN;
  int32_t rh_min = INT32_MAX, rh_max = INT32_MIN;
  int32_t to_min = INT32_MAX, to_max = INT32_MIN;
  for (size_t i = 0U; i < StuckHistory::kLength; ++i) {
    if (h.t_in[i] == INT16_MIN || h.rh_in[i] == UINT16_MAX ||
        h.t_out[i] == INT16_MIN) {
      return false;  // failed reads are reported by their own flags
    }
    if (h.t_in[i] < tin_min) tin_min = h.t_in[i];
    if (h.t_in[i] > tin_max) tin_max = h.t_in[i];
    if (h.rh_in[i] < rh_min) rh_min = h.rh_in[i];
    if (h.rh_in[i] > rh_max) rh_max = h.rh_in[i];
    if (h.t_out[i] < to_min) to_min = h.t_out[i];
    if (h.t_out[i] > to_max) to_max = h.t_out[i];
  }
  // 0.02 C / 0.2 %RH of spread inside, at least 1 C of spread outside.
  return (tin_max - tin_min) <= 2 && (rh_max - rh_min) <= 20 &&
         (to_max - to_min) >= 100;
}

size_t base64EncodedSize(size_t length) { return ((length + 2U) / 3U) * 4U; }

size_t base64Encode(const uint8_t* data, size_t length, char* out,
                    size_t capacity) {
  static const char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const size_t needed = base64EncodedSize(length);
  if (out == nullptr || capacity < needed + 1U) return 0U;
  size_t o = 0U;
  size_t i = 0U;
  while (i + 2U < length) {
    const uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                       (static_cast<uint32_t>(data[i + 1U]) << 8) |
                       data[i + 2U];
    out[o++] = kAlphabet[(v >> 18) & 63U];
    out[o++] = kAlphabet[(v >> 12) & 63U];
    out[o++] = kAlphabet[(v >> 6) & 63U];
    out[o++] = kAlphabet[v & 63U];
    i += 3U;
  }
  if (i < length) {
    uint32_t v = static_cast<uint32_t>(data[i]) << 16;
    if (i + 1U < length) v |= static_cast<uint32_t>(data[i + 1U]) << 8;
    out[o++] = kAlphabet[(v >> 18) & 63U];
    out[o++] = kAlphabet[(v >> 12) & 63U];
    out[o++] = (i + 1U < length) ? kAlphabet[(v >> 6) & 63U] : '=';
    out[o++] = '=';
  }
  out[o] = '\0';
  return o;
}

}  // namespace BeeCoolerLogic
