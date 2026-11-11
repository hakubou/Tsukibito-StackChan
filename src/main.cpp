// 平安俳句ｽﾀｯｸﾁｬﾝ
//  - 平安顔（引目鉤鼻・置眉）の Avatar を表示する
//  - 画面をタッチすると、ずんだもんの声でセリフを順に話す（口パク付き）
//  音声: VOICEVOX:ずんだもん（tools/gen_voices.py で生成して埋め込み）
#include <M5Unified.h>
#include <Avatar.h>
#include "HeianFace.h"
#include "voice_table.h"

using namespace m5avatar;

// platformio.ini の board_build.embed_files で埋め込んだ音声データ
extern const uint8_t voicesStart[] asm("_binary_assets_voices_bin_start");
extern const uint8_t voicesEnd[] asm("_binary_assets_voices_bin_end");

Avatar* avatar;
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
  avatar->setExpression(clip.expression);
  avatar->setSpeechText(clip.caption);
  speaking = &clip;
  speakStartMs = millis();
  Serial.printf("speak: %s\n", clip.id);
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

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Display.setBrightness(128);
  Serial.println("Heian Stack-chan start");

  avatar = new Avatar(heian::createFace());
  avatar->setColorPalette(heian::createPalette());
  avatar->setSpeechFont(&fonts::lgfxJapanGothic_12);
  avatar->init(16);  // 16bit カラー（紅や頬紅を出すため）

  setupVoice();
  delay(500);
  clipIndex = 0;
  speak(clipIndex);
}

void loop() {
  M5.update();

  if (M5.Touch.getDetail().wasPressed()) {
    clipIndex = (clipIndex + 1) % kVoiceClipCount;
    speak(clipIndex);
  }

  float target = 0;
  if (speaking) {
    if (M5.Speaker.isPlaying()) {
      target = mouthFromVoice();
    } else {  // 話し終わったら少し間をおいて字幕と表情を戻す
      if (millis() - speakStartMs > speaking->samples * 1000 / kVoiceSampleRate + 800) {
        speaking = nullptr;
        avatar->setSpeechText("");
        avatar->setExpression(Expression::Neutral);
      }
    }
  }
  mouth += (target - mouth) * 0.5f;  // 急に開閉しないようならす
  avatar->setMouthOpenRatio(mouth);

  delay(20);
}
