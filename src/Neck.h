// 首（Feetech SCS0009 ×2）を動かす
//  - 半二重シリアル：TX=G17 → ショットキーダイオード → SIG ← RX=G18、SIG は 1kΩ で 3.3V にプルアップ
//    （docs/neck_wiring.md。回路は StackChan/docs/servo_board.md と同じ）
//  - 自分の送信がそのまま RX に返ってくる（エコー）ので読み捨てる
//  - 電源のコンデンサがない構成でも電圧が落ち込みにくいよう、動きには必ず時間をかけ、
//    急な全速移動はしない（最低 kMinMoveMs）
//  - 角度は「正面＝0°」の度で扱う。パン：+で左（ロボットから見て）、チルト：+で下（お辞儀）
#pragma once
#include <M5Unified.h>

namespace neck {

constexpr int kTxPin = 17, kRxPin = 18;
constexpr uint32_t kBaud = 1000000;
constexpr uint8_t kPanId = 1, kTiltId = 2;

// 向きが逆だったら -1 にする
constexpr int kPanDir = 1, kTiltDir = 1;
// 可動範囲（度）。筐体や烏帽子が当たらない範囲に絞る
constexpr float kPanMin = -40, kPanMax = 40;
constexpr float kTiltMin = -10, kTiltMax = 30;
// 正面の位置（0〜1023）。組み付けのずれはここで直す
constexpr int kPanCenter = 512, kTiltCenter = 512;

constexpr float kUnitsPerDeg = 1024.0f / 300.0f;  // SCS0009：300°で 0〜1023
constexpr uint32_t kMinMoveMs = 250;              // どんな小さな動きでもこれ以上かける
constexpr float kMaxDegPerSec = 120;              // 速さの上限

// SCS0009 のレジスタ（SCS プロトコル、2バイト値はビッグエンディアン）
constexpr uint8_t kRegTorque = 0x28, kRegGoalPos = 0x2A, kRegPresentPos = 0x38;
constexpr uint8_t kInstRead = 0x02, kInstWrite = 0x03;

namespace detail {
inline HardwareSerial& bus = Serial1;
inline float panDeg = 0, tiltDeg = 0;

// 送信して、エコーを読み捨てる。replyLen > 0 なら続く返事を reply に読む
inline bool transact(uint8_t id, uint8_t inst, const uint8_t* params, uint8_t n,
                     uint8_t* reply = nullptr, uint8_t replyLen = 0) {
  uint8_t pkt[32];
  pkt[0] = 0xFF; pkt[1] = 0xFF; pkt[2] = id; pkt[3] = n + 2; pkt[4] = inst;
  uint8_t sum = id + n + 2 + inst;
  for (uint8_t i = 0; i < n; ++i) { pkt[5 + i] = params[i]; sum += params[i]; }
  pkt[5 + n] = ~sum;
  const size_t len = 6 + n;

  while (bus.available()) bus.read();
  bus.write(pkt, len);
  bus.flush();
  // エコー＋返事を待つ（1Mbps なので数百µsで届く）
  const size_t want = len + (replyLen ? replyLen : 0);
  uint8_t buf[48];
  size_t got = 0;
  const uint32_t t0 = micros();
  while (got < want && micros() - t0 < 3000) {
    if (bus.available()) buf[got++] = bus.read();
  }
  if (!replyLen) return true;
  if (got < want) return false;
  const uint8_t* r = buf + len;  // エコーの後ろが返事
  if (r[0] != 0xFF || r[1] != 0xFF || r[2] != id) return false;
  memcpy(reply, r, replyLen);
  return true;
}

inline void writeReg(uint8_t id, uint8_t addr, const uint8_t* data, uint8_t n) {
  uint8_t p[16];
  p[0] = addr;
  memcpy(p + 1, data, n);
  transact(id, kInstWrite, p, n + 1);
  delayMicroseconds(300);  // サーボの返事（6バイト）が流れ切るのを待つ
  while (bus.available()) bus.read();
}

inline void writePos(uint8_t id, int pos, uint16_t ms) {
  pos = constrain(pos, 0, 1023);
  const uint8_t d[6] = {(uint8_t)(pos >> 8), (uint8_t)pos, (uint8_t)(ms >> 8), (uint8_t)ms, 0, 0};
  writeReg(id, kRegGoalPos, d, 6);
}

inline uint16_t moveMs(float fromDeg, float toDeg, uint32_t ms) {
  const uint32_t byLimit = fabsf(toDeg - fromDeg) / kMaxDegPerSec * 1000;
  return (uint16_t)std::max<uint32_t>({ms, byLimit, kMinMoveMs});
}
}  // namespace detail

// 現在位置（0〜1023）を読む。読めなければ -1
inline int readPos(uint8_t id) {
  const uint8_t p[2] = {kRegPresentPos, 2};
  uint8_t r[8];
  if (!detail::transact(id, kInstRead, p, 2, r, 8)) return -1;  // FF FF ID 04 ERR H L SUM
  return (r[5] << 8) | r[6];
}

// 指定の角度へ ms かけて動かす（速さの上限を超える場合は自動でゆっくりにする）
inline void look(float panDeg, float tiltDeg, uint32_t ms = 400) {
  panDeg = constrain(panDeg, kPanMin, kPanMax);
  tiltDeg = constrain(tiltDeg, kTiltMin, kTiltMax);
  const uint16_t pm = detail::moveMs(detail::panDeg, panDeg, ms);
  const uint16_t tm = detail::moveMs(detail::tiltDeg, tiltDeg, ms);
  detail::writePos(kPanId, kPanCenter + (int)(kPanDir * panDeg * kUnitsPerDeg), pm);
  detail::writePos(kTiltId, kTiltCenter + (int)(kTiltDir * tiltDeg * kUnitsPerDeg), tm);
  detail::panDeg = panDeg;
  detail::tiltDeg = tiltDeg;
}

inline float pan() { return detail::panDeg; }
inline float tilt() { return detail::tiltDeg; }

// 起動：2個のサーボを探してトルクを入れ、ゆっくり正面へ
inline bool begin() {
  M5.Power.setExtOutput(true);  // M-Bus の 5V を出す
  detail::bus.begin(kBaud, SERIAL_8N1, kRxPin, kTxPin);
  delay(50);
  const int p1 = readPos(kPanId), p2 = readPos(kTiltId);
  Serial.printf("neck: pan(ID%d)=%d tilt(ID%d)=%d\n", kPanId, p1, kTiltId, p2);
  const uint8_t on = 1;
  detail::writeReg(kPanId, kRegTorque, &on, 1);
  delay(20);
  detail::writeReg(kTiltId, kRegTorque, &on, 1);
  // 起動直後の位置から正面へ 1.5 秒かけて（2個を少しずらして）動かす
  if (p1 >= 0) detail::panDeg = kPanDir * (p1 - kPanCenter) / kUnitsPerDeg;
  if (p2 >= 0) detail::tiltDeg = kTiltDir * (p2 - kTiltCenter) / kUnitsPerDeg;
  detail::writePos(kPanId, kPanCenter, 1500);
  delay(200);
  detail::writePos(kTiltId, kTiltCenter, 1500);
  detail::panDeg = detail::tiltDeg = 0;
  return p1 >= 0 && p2 >= 0;
}

// ---- 決まった動き（update() を毎ループ呼ぶと進む） ----
enum class Motion { None, Bow, Nod, LookAround };

namespace detail {
inline Motion motion = Motion::None;
inline int step = 0;
inline uint32_t nextAt = 0;
inline float basePan = 0, baseTilt = 0;
}  // namespace detail

inline void play(Motion m) {
  detail::motion = m;
  detail::step = 0;
  detail::nextAt = 0;
  detail::basePan = detail::panDeg;
  detail::baseTilt = detail::tiltDeg;
}
inline bool busy() { return detail::motion != Motion::None; }

inline void update() {
  using namespace detail;
  if (motion == Motion::None || millis() < nextAt) return;
  struct Key { float pan, tilt; uint32_t ms, hold; };
  static const Key bow[] = {{0, 25, 700, 900}, {0, 0, 800, 0}};          // お辞儀
  static const Key nod[] = {{0, 10, 250, 50}, {0, 0, 300, 0}};           // 頷き
  static const Key around[] = {{25, 0, 900, 400}, {-25, 0, 1400, 400}, {0, 0, 900, 0}};  // 見渡す
  const Key* keys = motion == Motion::Bow ? bow : motion == Motion::Nod ? nod : around;
  const int count = motion == Motion::Bow ? 2 : motion == Motion::Nod ? 2 : 3;
  if (step >= count) { motion = Motion::None; return; }
  const Key& k = keys[step++];
  const float p = motion == Motion::LookAround ? k.pan : basePan + k.pan;
  look(p, baseTilt + k.tilt, k.ms);
  nextAt = millis() + k.ms + k.hold;
}

}  // namespace neck
