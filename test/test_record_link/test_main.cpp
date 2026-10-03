#include <unity.h>

#include <math.h>
#include <string.h>

#include <BeeCoolerLink.h>
#include <BeeCoolerRecord.h>

using namespace BeeCoolerLink;
using namespace BeeCoolerRecord;

void setUp() {}
void tearDown() {}

static RecordV1 sampleRecord(uint32_t ts) {
  RecordV1 r;
  initInvalid(r);
  r.ts = ts;
  r.t_in = 3150;
  r.rh_in = 6200;
  r.vbat_mv = 3912;
  r.flags = setPowerLevel(kColdBoot, 2);
  return r;
}

void test_layout_sizes() {
  TEST_ASSERT_EQUAL(24, sizeof(RecordV1));
  TEST_ASSERT_EQUAL(32, sizeof(RingSlot));
  TEST_ASSERT_EQUAL(32, sizeof(RawHeader));
  TEST_ASSERT_EQUAL(64, sizeof(ExtendedRecord));
}

void test_sentinels_and_flags() {
  RecordV1 r;
  initInvalid(r);
  TEST_ASSERT_EQUAL_INT16(INT16_MIN, r.t_in);
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, r.rh_out);
  TEST_ASSERT_EQUAL_UINT8(0xFF, r.band_db[3]);
  uint16_t f = setPowerLevel(kRtcInvalid, 3);
  TEST_ASSERT_EQUAL_UINT8(3, getPowerLevel(f));
  TEST_ASSERT_TRUE(f & kRtcInvalid);
  f = setPowerLevel(f, 1);
  TEST_ASSERT_EQUAL_UINT8(1, getPowerLevel(f));
}

void test_band_db_encoding() {
  TEST_ASSERT_EQUAL_UINT8(200, encodeBandDb(0.0f));
  TEST_ASSERT_EQUAL_UINT8(0, encodeBandDb(-100.0f));
  TEST_ASSERT_EQUAL_UINT8(0xFF, encodeBandDb(NAN));
  TEST_ASSERT_EQUAL_UINT8(254, encodeBandDb(1000.0f));
  TEST_ASSERT_EQUAL_FLOAT(-12.5f, decodeBandDb(encodeBandDb(-12.5f)));
  TEST_ASSERT_TRUE(isnan(decodeBandDb(0xFF)));
}

void test_crc_vectors() {
  const uint8_t v[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16(v, sizeof(v)));  // CCITT-FALSE
  TEST_ASSERT_EQUAL_HEX8(0xF4, crc8(v, sizeof(v)));      // CRC-8 poly 0x07
}

void test_slot_roundtrip_and_corruption() {
  RingSlot slot;
  buildSlot(77, sampleRecord(1000), slot);
  TEST_ASSERT_TRUE(validateSlot(slot));
  slot.record.t_in ^= 1;
  TEST_ASSERT_FALSE(validateSlot(slot));
  memset(&slot, 0xFF, sizeof(slot));  // erased flash
  TEST_ASSERT_FALSE(validateSlot(slot));
}

void test_data_roundtrip() {
  RecordV1 recs[4];
  for (int i = 0; i < 4; ++i) recs[i] = sampleRecord(1000 + i);
  Header h = {};
  h.node_id = 1;
  h.boot_id = 9;
  h.frame_seq = 513;
  h.attempt = 2;
  h.first_seq = 100000;
  uint8_t buf[kMaxFrameSize];
  size_t n = 0;
  TEST_ASSERT_EQUAL(Result::kOk, encodeData(h, recs, 4, buf, sizeof(buf), n));
  TEST_ASSERT_EQUAL(113, n);
  Frame f;
  TEST_ASSERT_EQUAL(Result::kOk, decodeFrame(buf, n, f));
  TEST_ASSERT_FALSE(f.is_ack);
  TEST_ASSERT_EQUAL(FrameType::kData, f.header.type);
  TEST_ASSERT_EQUAL_UINT32(100000, f.header.first_seq);
  TEST_ASSERT_EQUAL_UINT16(513, f.header.frame_seq);
  TEST_ASSERT_EQUAL_UINT8(4, f.header.n_rec);
  TEST_ASSERT_EQUAL_MEMORY(recs, f.payload, sizeof(recs));
}

void test_status_hello_sizes_and_roundtrip() {
  Header h = {};
  h.node_id = 1;
  h.first_seq = 42;
  Status s = {};
  s.record = sampleRecord(5);
  s.backlog = 12;
  s.sd_free_mb = 15000;
  s.rssi = 71;
  s.vbat_tx_mv = 3800;
  s.fw_version = 0x0102;
  uint8_t buf[kMaxFrameSize];
  size_t n = 0;
  TEST_ASSERT_EQUAL(Result::kOk, encodeStatus(h, s, buf, sizeof(buf), n));
  TEST_ASSERT_EQUAL(53, n);
  Frame f;
  TEST_ASSERT_EQUAL(Result::kOk, decodeFrame(buf, n, f));
  Status out;
  TEST_ASSERT_EQUAL(Result::kOk, decodeStatus(f.payload, f.payload_length, out));
  TEST_ASSERT_EQUAL_UINT16(12, out.backlog);
  TEST_ASSERT_EQUAL_UINT16(0x0102, out.fw_version);
  TEST_ASSERT_EQUAL_UINT32(5, out.record.ts);

  Hello hello = {0x0102, 3, 1, 999, 17};
  TEST_ASSERT_EQUAL(Result::kOk, encodeHello(h, hello, buf, sizeof(buf), n));
  TEST_ASSERT_EQUAL(27, n);
  TEST_ASSERT_EQUAL(Result::kOk, decodeFrame(buf, n, f));
  Hello ho;
  TEST_ASSERT_EQUAL(Result::kOk, decodeHello(f.payload, f.payload_length, ho));
  TEST_ASSERT_EQUAL_UINT32(999, ho.last_seq);
  TEST_ASSERT_EQUAL_UINT16(17, ho.boot_count);
}

void test_ack_roundtrip() {
  Ack a = {1, 513, 123456, 1790186460U, 71, -9, 0};
  uint8_t buf[kMaxFrameSize];
  size_t n = 0;
  TEST_ASSERT_EQUAL(Result::kOk, encodeAck(a, buf, sizeof(buf), n));
  TEST_ASSERT_EQUAL(21, n);
  Frame f;
  TEST_ASSERT_EQUAL(Result::kOk, decodeFrame(buf, n, f));
  TEST_ASSERT_TRUE(f.is_ack);
  TEST_ASSERT_EQUAL_UINT32(123456, f.ack.acked_up_to);
  TEST_ASSERT_EQUAL_UINT32(1790186460U, f.ack.epoch);
  TEST_ASSERT_EQUAL_INT8(-9, f.ack.snr);
}

void test_set_attempt_refreshes_crc() {
  Header h = {};
  h.node_id = 1;
  h.frame_seq = 9;
  h.attempt = 1;
  Hello hello = {1, 0, 1, 5, 1};
  uint8_t buf[kMaxFrameSize];
  size_t n = 0;
  encodeHello(h, hello, buf, sizeof(buf), n);
  TEST_ASSERT_EQUAL(Result::kOk, setAttempt(buf, n, 3));
  Frame f;
  TEST_ASSERT_EQUAL(Result::kOk, decodeFrame(buf, n, f));
  TEST_ASSERT_EQUAL_UINT8(3, f.header.attempt);
  TEST_ASSERT_EQUAL_UINT16(9, f.header.frame_seq);
}

void test_corrupted_crc_rejected() {
  Ack a = {1, 1, 1, 1, 1, 1, 0};
  uint8_t buf[kAckSize];
  size_t n = 0;
  encodeAck(a, buf, sizeof(buf), n);
  buf[8] ^= 0x40;
  Frame f;
  TEST_ASSERT_EQUAL(Result::kBadCrc, decodeFrame(buf, n, f));
}

void test_stream_parser_resync_and_split() {
  Ack a = {1, 7, 55, 1000, 60, 5, 0};
  uint8_t ack[kAckSize];
  size_t an = 0;
  encodeAck(a, ack, sizeof(ack), an);

  Header h = {};
  h.node_id = 1;
  Hello hello = {1, 0, 1, 5, 1};
  uint8_t hf[kMaxFrameSize];
  size_t hn = 0;
  encodeHello(h, hello, hf, sizeof(hf), hn);

  // garbage (including a stray magic byte), a corrupted frame, then two good
  // frames back to back.
  uint8_t stream[200];
  size_t len = 0;
  const uint8_t junk[] = {0x00, 0xBE, 0x03, 0x55};
  memcpy(stream + len, junk, sizeof(junk));
  len += sizeof(junk);
  uint8_t bad[kAckSize];
  memcpy(bad, ack, kAckSize);
  bad[10] ^= 0xFF;
  memcpy(stream + len, bad, kAckSize);
  len += kAckSize;
  memcpy(stream + len, ack, an);
  len += an;
  memcpy(stream + len, hf, hn);
  len += hn;

  StreamParser p;
  int good = 0;
  for (size_t i = 0; i < len; ++i) {
    bool got = p.feed(stream[i]);
    while (got) {
      Frame f;
      TEST_ASSERT_EQUAL(Result::kOk, decodeFrame(p.frame(), p.frameSize(), f));
      ++good;
      got = p.poll();
    }
  }
  TEST_ASSERT_EQUAL(2, good);
  TEST_ASSERT_GREATER_THAN_UINT32(0, p.crcFailures());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_layout_sizes);
  RUN_TEST(test_sentinels_and_flags);
  RUN_TEST(test_band_db_encoding);
  RUN_TEST(test_crc_vectors);
  RUN_TEST(test_slot_roundtrip_and_corruption);
  RUN_TEST(test_data_roundtrip);
  RUN_TEST(test_status_hello_sizes_and_roundtrip);
  RUN_TEST(test_ack_roundtrip);
  RUN_TEST(test_set_attempt_refreshes_crc);
  RUN_TEST(test_corrupted_crc_rejected);
  RUN_TEST(test_stream_parser_resync_and_split);
  return UNITY_END();
}
