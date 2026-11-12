// tools/gen_voices.py が自動生成。手で編集しないこと
// 音声: VOICEVOX:ちび式じい（ノーマル）
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
  {"greet", "ようこそじゃ", heian::Expr::Happy, 0, 103083},
  {"ask", "一句聞かせてくだされ", heian::Expr::Neutral, 103083, 54101},
  {"praise", "見事な句じゃ", heian::Expr::Happy, 157184, 46421},
  {"writing", "したためておる", heian::Expr::Sleepy, 203605, 80725},
  {"give", "どうぞじゃ", heian::Expr::Happy, 284330, 70656},
  {"paperout", "紙切れじゃ", heian::Expr::Sad, 354986, 50688},
};
constexpr int kVoiceClipCount = sizeof(kVoiceClips) / sizeof(kVoiceClips[0]);
