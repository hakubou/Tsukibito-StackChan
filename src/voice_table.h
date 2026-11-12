// tools/gen_voices.py が自動生成。手で編集しないこと
// 音声: VOICEVOX:ずんだもん（ノーマル）
#pragma once
#include "HeianFace.h"

constexpr uint32_t kVoiceSampleRate = 16000;

struct VoiceClip {
  const char* id;
  const char* caption;
  heian::Expr expression;
  uint32_t offset;   // voices.bin 先頭からのサンプル位置
  uint32_t samples;  // サンプル数
};

const VoiceClip kVoiceClips[] = {
  {"greet", "こんにちはなのだ！", heian::Expr::Happy, 0, 67584},
  {"ask", "一句ほしいのだ", heian::Expr::Neutral, 67584, 40789},
  {"praise", "素敵な句なのだ！", heian::Expr::Happy, 108373, 29525},
  {"writing", "書いてるのだ…", heian::Expr::Sleepy, 137898, 61269},
  {"give", "どうぞなのだ！", heian::Expr::Happy, 199167, 45739},
  {"paperout", "紙切れなのだ…", heian::Expr::Sad, 244906, 44715},
};
constexpr int kVoiceClipCount = sizeof(kVoiceClips) / sizeof(kVoiceClips[0]);
