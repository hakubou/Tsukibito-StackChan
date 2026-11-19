// 平安俳句ｽﾀｯｸﾁｬﾝ
//  - 平安顔（引目鉤鼻・置眉）を表示する（HeianFace.h の独自描画）
//  - 内蔵カメラで顔を検出し、首と瞳をその顔へ向ける（顔追従。シリアルの t でON/OFF）
//  - 画面をタッチすると、ちび式じいの声でセリフを順に話す（口パク付き）
//  - シリアルに s を送ると、カメラ画像を1枚 base64 で出す（デバッグ用）
//  - 首（SCS0009×2）：起動時にトルクを入れてゆっくり正面へ。e でトルクON / d でOFF。動作確認は c 正面 / l 左 / r 右 / u 上 / b お辞儀 / n 頷き / a 見渡す / p 位置を読む）
//    ID：i 探す / I2 つながっている1個を ID 2 に書き換える（サーボは1個だけつなぐ）/ X ID 1 と 2 を入れ替える
//  音声: VOICEVOX:ちび式じい（tools/gen_voices.py で生成して埋め込み）
#include <M5Unified.h>
#include "HeianFace.h"
#include "voice_table.h"
#include "FaceSensor.h"
#include "Neck.h"

// platformio.ini の board_build.embed_files で埋め込んだ音声データ
extern const uint8_t voicesStart[] asm("_binary_assets_voices_bin_start");
extern const uint8_t voicesEnd[] asm("_binary_assets_voices_bin_end");

// カメラ画像の左右が鏡像かどうか（顔から逃げるように首が回るなら true/false を入れ替える）
constexpr bool kCameraMirrored = false;

// ---- 顔追従の設定 ----
constexpr float kCamHFovDeg = 60, kCamVFovDeg = 45;  // カメラ（GC0308）の画角の目安
constexpr float kTrackGain = 0.6f;     // 1回でずれの何割ぶん首を回すか（大きいと速いが行き過ぎやすい）
constexpr float kTrackDeadband = 0.08f;  // 顔が画像の中心からこの割合以内なら首は動かさない
constexpr uint32_t kTrackMoveMs = 250;   // 1回の首の動きにかける時間
constexpr uint32_t kReturnAfterMs = 3000;  // 顔を見失ってから正面に戻るまで
bool tracking = true;

heian::Renderer face;
const int16_t* pcm = nullptr;  // 全セリフの PCM（2バイト境界にそろえたもの）

int clipIndex = -1;
const VoiceClip* speaking = nullptr;  // 再生中のセリフ
uint32_t speakStartMs = 0;
float mouth = 0;

void setupVoice() {
  const size_t bytes = voicesEnd - voicesStart;
  if (reinterpret_cast<uintptr_t>(voicesStart) % 2 == 0) {
    pcm = reinterpret_cast<const int16_t*>(voicesStart);
  } else {  // 境界がずれていたら PSRAM にコピーして使う
    auto* buf = static_cast<int16_t*>(ps_malloc(bytes));
    memcpy(buf, voicesStart, bytes);
    pcm = buf;
  }
  Serial.printf("voices: %u bytes, %d clips\n", (unsigned)bytes, kVoiceClipCount);

  M5.Speaker.begin();
  M5.Speaker.setVolume(180);
}

void speak(int index) {
  const VoiceClip& clip = kVoiceClips[index];
  M5.Speaker.stop();
  M5.Speaker.playRaw(pcm + clip.offset, clip.samples, kVoiceSampleRate, false, 1, 0, true);
  face.setExpr(clip.expression);
  speaking = &clip;
  speakStartMs = millis();
  Serial.printf("speak: %s (%s)\n", clip.id, clip.caption);
  if (strcmp(clip.id, "give") == 0) neck::play(neck::Motion::Bow);  // 手渡しのセリフではお辞儀
}

// ID の確認（i）と書き換え（I<新ID>、強制は F<新ID>）。サーボは1個だけつなぐこと
void idCommand(char cmd) {
  int found[8];
  const int n = neck::scan(found, 8);
  Serial.printf("id scan: %d found", n);
  for (int i = 0; i < n; ++i) Serial.printf(" [%d]", found[i]);
  Serial.println();
  if (cmd == 'i') return;
  if (cmd == 'X') {  // ID 1 と 2 を入れ替える（2個つないだまま。一時的に ID 3 を使う）
    bool has1 = false, has2 = false, has3 = false;
    for (int i = 0; i < n; ++i) { has1 |= found[i] == 1; has2 |= found[i] == 2; has3 |= found[i] == 3; }
    if (!has1 || !has2 || has3) { Serial.println("id swap: need ID1 and ID2 connected, and no ID3"); return; }
    const bool a = neck::changeId(1, 3), b = neck::changeId(2, 1), c = neck::changeId(3, 2);
    Serial.printf("id swap: 1->3 %s, 2->1 %s, 3->2 %s\n", a ? "OK" : "NG", b ? "OK" : "NG", c ? "OK" : "NG");
    return;
  }

  const int newId = Serial.parseInt();  // I や F の後ろの数字
  if (newId < 1 || newId > 253) { Serial.println("id: new ID must be 1-253"); return; }
  if (cmd == 'I') {
    if (n != 1) { Serial.println("id: connect exactly ONE servo (use F<id> only if reading fails)"); return; }
    if (found[0] == newId) { Serial.printf("id: already %d\n", newId); return; }
    const bool ok = neck::changeId(found[0], newId);
    Serial.printf("id: %d -> %d %s\n", found[0], newId, ok ? "OK" : "NG (no reply with new ID)");
  } else {  // F：返事が読めないときの最終手段。ブロードキャストで書き換える
    const bool ok = neck::changeId(neck::kBroadcastId, newId);
    Serial.printf("id: broadcast -> %d %s\n", newId, ok ? "OK" : "sent (reply not readable)");
  }
}

// シリアルの1文字コマンド（首の動作確認・ID書き換え・カメラ画像の取り出し）
void handleSerial() {
  if (!Serial.available()) return;
  const char c = Serial.read();
  if (c == 'i' || c == 'I' || c == 'F' || c == 'X') { idCommand(c); return; }
  switch (c) {
    case 's': face_sensor::dumpSnapshot(); break;
    case 'e': neck::enable(); Serial.println("neck: torque on"); break;
    case 'd': neck::disable(); Serial.println("neck: torque off"); break;
    case 'c': neck::look(0, 0); break;
    case 'l': neck::look(30, 0); break;
    case 'r': neck::look(-30, 0); break;
    case 'u': neck::look(0, -10); break;
    case 'b': neck::play(neck::Motion::Bow); break;
    case 'n': neck::play(neck::Motion::Nod); break;
    case 'a': neck::play(neck::Motion::LookAround); break;
    case 't': tracking = !tracking; Serial.printf("tracking: %s\n", tracking ? "on" : "off"); if (!tracking) neck::look(0, 0, 1000); break;
    case 'p': Serial.printf("neck pos: pan=%d tilt=%d\n", neck::readPos(neck::kPanId), neck::readPos(neck::kTiltId)); break;
  }
}

// 再生位置の前後 20ms の音量から口の開きを決める
float mouthFromVoice() {
  const uint32_t pos = (uint64_t)(millis() - speakStartMs) * kVoiceSampleRate / 1000;
  if (pos >= speaking->samples) return 0;
  constexpr uint32_t kWindow = kVoiceSampleRate / 50;
  const int16_t* p = pcm + speaking->offset + pos;
  const uint32_t n = std::min(kWindow, speaking->samples - pos);
  uint32_t sum = 0;
  for (uint32_t i = 0; i < n; ++i) sum += abs(p[i]);
  const float level = (float)sum / n / 3200.0f;  // 3200 は経験値（ちび式じいの声の上位1割でほぼ全開）
  return std::min(1.0f, level);
}

// 顔追従：見つけた顔が画像の中心に来るように首を回し、瞳もその顔へ向ける
//  - カメラは頭に付いているので、首が動いている最中の画像はぶれる。首が止まってから撮った画像だけを使う
//  - 顔を見失って kReturnAfterMs たったら、ゆっくり正面に戻る
void trackFace() {
  static uint32_t lastFrame = 0, settleUntil = 0, lostAt = 0;
  static bool atHome = true;
  const auto f = face_sensor::get();
  const uint32_t now = millis();
  const bool seen = f.count > 0 && now - f.atMs < 1500;

  // 画像の中心からのずれ（-1〜1）。ex は +でロボットの右、ey は +で下
  float ex = 0, ey = 0;
  if (seen) {
    ex = (f.x - 160) / 160.0f;
    ey = (f.y - 120) / 120.0f;
    if (kCameraMirrored) ex = -ex;
  }
  // 瞳：画面の右＝ロボットの左なので、左右を反転して渡す
  face.lookAt(-ex, ey, seen);

  if (!tracking || !neck::isEnabled() || neck::busy()) return;
  if (seen) {
    atHome = false;
    lostAt = now;
    const uint32_t shotAt = f.atMs - f.detectMs;  // おおよその撮影時刻
    if (f.frames == lastFrame || shotAt < settleUntil) return;
    lastFrame = f.frames;
    if (fabsf(ex) < kTrackDeadband) ex = 0;
    if (fabsf(ey) < kTrackDeadband) ey = 0;
    if (ex == 0 && ey == 0) return;
    // パンは +で左、チルトは +で下（Neck.h）
    const float pan = neck::pan() - ex * kCamHFovDeg / 2 * kTrackGain;
    const float tilt = neck::tilt() + ey * kCamVFovDeg / 2 * kTrackGain;
    neck::look(pan, tilt, kTrackMoveMs);
    settleUntil = now + kTrackMoveMs + 80;
  } else if (!atHome && now - lostAt > kReturnAfterMs) {
    neck::look(0, 0, 1500);
    atHome = true;
  }
}

// S0 計測：2秒ごとに描画速度・顔検出の結果・メモリをシリアルに出す
void reportStats() {
  static uint32_t next = 0, lastFrames = 0, lastDetects = 0;
  if (millis() < next) return;
  next = millis() + 2000;
  const uint32_t frames = face.frameCount;
  const auto f = face_sensor::get();
  Serial.printf("draw %.1ffps | detect %.1f/s %ums | faces %d", (frames - lastFrames) / 2.0f,
                (f.frames - lastDetects) / 2.0f, (unsigned)f.detectMs, f.count);
  if (f.count) Serial.printf(" at (%d,%d) size %dx%d", f.x, f.y, f.w, f.h);
  Serial.printf(" | heap %u psram %u\n", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  lastFrames = frames;
  lastDetects = f.frames;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Display.setBrightness(128);
  Serial.println("Heian Stack-chan start");

  face.begin();
  setupVoice();
#ifndef NO_CAMERA
  const bool cam = face_sensor::begin();
#else
  const bool cam = false;
#endif
  Serial.printf("camera: %s\n", cam ? "ok" : "NG");
  if (neck::begin()) {
    neck::enable();  // 正面の位置は調整済みなので、起動時にゆっくり正面へ向ける
    Serial.println("neck: ok (torque on)");
  } else {
    Serial.println("neck: servo not found");
  }
  delay(500);
  clipIndex = 0;
  speak(clipIndex);
}

void loop() {
  M5.update();
  reportStats();
  trackFace();

  handleSerial();
  neck::update();

  if (M5.Touch.getDetail().wasPressed()) {
    Serial.println("touch");
    clipIndex = (clipIndex + 1) % kVoiceClipCount;
    speak(clipIndex);
  }

  // 顔を見つけている間は笑顔にする（検出できているかが画面で分かるように）
  static bool smiling = false;
  const auto f = face_sensor::get();
  const bool seen = f.count > 0 && millis() - f.atMs < 1000;
  if (!speaking && seen != smiling) {
    smiling = seen;
    face.setExpr(seen ? heian::Expr::Happy : heian::Expr::Neutral);
  }

  float target = 0;
  if (speaking) {
    if (M5.Speaker.isPlaying()) {
      target = mouthFromVoice();
    } else if (millis() - speakStartMs > speaking->samples * 1000 / kVoiceSampleRate + 800) {
      speaking = nullptr;  // 話し終わったら少し間をおいて表情を戻す
      face.setExpr(heian::Expr::Neutral);
    }
  }
  mouth += (target - mouth) * 0.5f;  // 急に開閉しないようならす
  face.setMouthOpen(mouth);

  delay(20);
}
