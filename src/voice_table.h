// tools/gen_voices.py が自動生成。手で編集しないこと
// 音声: VOICEVOX:ずんだもん（ノーマル）
#pragma once
#include <Avatar.h>

constexpr uint32_t kVoiceSampleRate = 16000;

struct VoiceClip {
  const char* id;
  const char* caption;
  m5avatar::Expression expression;
  uint32_t offset;   // voices.bin 先頭からのサンプル位置
  uint32_t samples;  // サンプル数
};

const VoiceClip kVoiceClips[] = {
  {"greet", "こんにちはなのだ！", m5avatar::Expression::Happy, 0, 67584},
  {"ask", "一句ほしいのだ", m5avatar::Expression::Neutral, 67584, 40789},
  {"praise", "素敵な句なのだ！", m5avatar::Expression::Happy, 108373, 29525},
  {"writing", "書いてるのだ…", m5avatar::Expression::Sleepy, 137898, 61269},
  {"give", "どうぞなのだ！", m5avatar::Expression::Happy, 199167, 45739},
  {"paperout", "紙切れなのだ…", m5avatar::Expression::Sad, 244906, 44715},
};
constexpr int kVoiceClipCount = sizeof(kVoiceClips) / sizeof(kVoiceClips[0]);
