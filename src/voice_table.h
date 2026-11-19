// tools/gen_voices.py が自動生成。手で編集しないこと
// 音声: VOICEVOX:ちび式じい（ノーマル）
#pragma once
#include "HeianFace.h"

constexpr uint32_t kVoiceSampleRate = 16000;

struct VoiceClip {
  const char* id;
  const char* category;  // 同じ category からランダムに選ぶ
  const char* caption;
  heian::Expr expression;
  uint32_t offset;   // voices.bin 先頭からのサンプル位置
  uint32_t samples;  // サンプル数
};

const VoiceClip kVoiceClips[] = {
  {"attract1", "attract", "垣間見していかれよ", heian::Expr::Happy, 0, 73899},
  {"attract2", "attract", "恋の歌を贈ってみぬか", heian::Expr::Happy, 73899, 60245},
  {"attract3", "attract", "平安の恋の入り口じゃ", heian::Expr::Neutral, 134144, 67925},
  {"attract4", "attract", "筆と木簡で歌を詠む", heian::Expr::Happy, 202069, 92331},
  {"watch1", "watch", "いま歌を詠んでおられる", heian::Expr::Neutral, 294400, 80043},
  {"watch2", "watch", "次はそなたの番じゃ", heian::Expr::Happy, 374443, 67243},
  {"watch3", "watch", "言葉を掬っておる", heian::Expr::Happy, 441686, 61269},
  {"greet1", "greet", "恋のお相手をお探しかな", heian::Expr::Happy, 502955, 83115},
  {"greet2", "greet", "ようこそじゃ", heian::Expr::Happy, 586070, 88576},
  {"kokoro1", "kokoro", "想いが届いたようじゃ", heian::Expr::Happy, 674646, 65024},
  {"kokoro2", "kokoro", "姫君の心も動いた", heian::Expr::Happy, 739670, 56491},
  {"tashinami1", "tashinami", "手ごわいのう", heian::Expr::Doubt, 796161, 66560},
  {"tashinami2", "tashinami", "返事が来ただけ上出来", heian::Expr::Neutral, 862721, 88917},
  {"nashi1", "nashi", "恋とはそういうものじゃ", heian::Expr::Sad, 951638, 79701},
  {"nashi2", "nashi", "次こそは", heian::Expr::Sad, 1031339, 75947},
  {"praise1", "praise", "よい歌じゃった", heian::Expr::Happy, 1107286, 50859},
  {"praise2", "praise", "みやびな歌じゃ", heian::Expr::Happy, 1158145, 81237},
  {"printed1", "printed", "紙を取ってくだされ", heian::Expr::Happy, 1239382, 88064},
  {"printed2", "printed", "持ってお帰りなされ", heian::Expr::Happy, 1327446, 95403},
  {"pet1", "pet", "くすぐったいわい", heian::Expr::Happy, 1422849, 41472},
  {"pet2", "pet", "烏帽子がずれる", heian::Expr::Neutral, 1464321, 68096},
  {"pet3", "pet", "気持ちがよいのう", heian::Expr::Sleepy, 1532417, 46933},
};
constexpr int kVoiceClipCount = sizeof(kVoiceClips) / sizeof(kVoiceClips[0]);
