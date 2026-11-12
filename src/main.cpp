// 平安俳句ｽﾀｯｸﾁｬﾝ
//  - 平安顔（引目鉤鼻・置眉）を表示する（HeianFace.h の独自描画）
//  - 内蔵カメラで顔を検出し、その方向へ瞳を向ける（S0：カメラの試作）
//  - 画面をタッチすると、ずんだもんの声でセリフを順に話す（口パク付き）
//  音声: VOICEVOX:ずんだもん（tools/gen_voices.py で生成して埋め込み）
#include <M5Unified.h>
#include "HeianFace.h"
#include "voice_table.h"
#include "FaceSensor.h"

// platformio.ini の board_build.embed_files で埋め込んだ音声データ
extern const uint8_t voicesStart[] asm("_binary_assets_voices_bin_start");
extern const uint8_t voicesEnd[] asm("_binary_assets_voices_bin_end");

// カメラ画像の左右が鏡像かどうか（顔を追う向きが逆なら true/false を入れ替える）
constexpr bool kCameraMirrored = false;

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
  const float level = (float)sum / n / 2000.0f;  // 2000 は経験値（ずんだもんの声の上位1割でほぼ全開）
  return std::min(1.0f, level);
}

// 見つけた顔の方へ瞳を向ける。見失って 1.5 秒たったら自動のきょろきょろに戻す
void followFace() {
  const auto f = face_sensor::get();
  if (f.count > 0 && millis() - f.atMs < 1500) {
    float gx = (f.x - 160) / 160.0f;
    if (!kCameraMirrored) gx = -gx;  // 画像の左＝ロボットの左＝画面の右
    face.lookAt(gx, (f.y - 120) / 120.0f, true);
  } else {
    face.lookAt(0, 0, false);
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
  delay(500);
  clipIndex = 0;
  speak(clipIndex);
}

void loop() {
  M5.update();
  reportStats();
  followFace();

  if (M5.Touch.getDetail().wasPressed()) {
    Serial.println("touch");
    clipIndex = (clipIndex + 1) % kVoiceClipCount;
    speak(clipIndex);
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
