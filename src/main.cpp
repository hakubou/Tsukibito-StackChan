// 付き人ｽﾀｯｸﾁｬﾝ（平安VR「掬って，詠んで，恋をして」の受付）
//  - 平安顔（引目鉤鼻・置眉）を表示する（HeianFace.h の独自描画）
//  - 内蔵カメラで顔を検出し、首と瞳をその顔へ向ける（顔追従。シリアルの t でON/OFF）
//  - 接客（下の「接客」の節）：
//    ・呼び込み：誰も遊んでいないときは 30〜60 秒おき（attract）。遊んでいる人がいるときは間をあけて、見ている人へ（watch）
//    ・顔を見つけたら声かけ（greet）。同じ人に続けて言わないよう間をあける
//    ・画面を撫でたら反応（pet）。触れたまま指を動かすか、0.6 秒以上触れたとき（触っていないのに出るタッチを拾わないため）
//    ・PC から結末が届いたら：結末のセリフ（kokoro / tashinami / nashi）→ よい歌じゃった（praise）
//      → 刷り上がったころにプリンタの方を向いて「紙を取ってくだされ」（printed）→ お辞儀
//  - PC とは USB シリアルでつなぐ。PC の tools/stackchan_bridge.py が、印刷プログラム（市橋さんの print_server.py）からの
//    知らせを {"type":"game_end","verdict":"kokoro"} の 1 行にして送ってくる（"game_start" が来れば「遊んでいる」にする）
//  - シリアルの 1 文字コマンド（動作確認）：
//    首：e トルクON / d OFF / c 正面 / h 待機の向き / l 左 / r 右 / u 上限 / k 5°上 / j 5°下 / b お辞儀 / n 頷き / a 見渡す / p 位置を読む
//    接客の試し：0 遊び始め / 1 結末 kokoro / 2 tashinami / 3 nashi / 7 撫でた / 8 声かけ / 9 呼び込み
//    音量：+ / -（覚えておく）　カメラ：s 画像を1枚 base64 で出す
//    ID：i 探す / I2 つながっている1個を ID 2 に書き換える（サーボは1個だけつなぐ）/ X ID 1 と 2 を入れ替える
//  音声: VOICEVOX:ちび式じい（voice/phrases.json → tools/gen_voices.py で生成して埋め込み）
#include <M5Unified.h>
#include "HeianFace.h"
#include "voice_table.h"
#include "FaceSensor.h"
#include "Neck.h"
#include <Preferences.h>

// platformio.ini の board_build.embed_files で埋め込んだ音声データ
extern const uint8_t voicesStart[] asm("_binary_assets_voices_bin_start");
extern const uint8_t voicesEnd[] asm("_binary_assets_voices_bin_end");

// カメラ画像の左右が鏡像かどうか（顔から逃げるように首が回るなら true/false を入れ替える）
constexpr bool kCameraMirrored = true;  // 実機で確認（2026-11-19）：画像は左右が鏡像

// ---- 顔追従の設定 ----
constexpr float kCamHFovDeg = 60, kCamVFovDeg = 45;  // カメラ（GC0308）の画角の目安
constexpr float kTrackGain = 0.6f;     // 1回でずれの何割ぶん首を回すか（大きいと速いが行き過ぎやすい）
constexpr float kTrackDeadband = 0.08f;  // 顔が画像の中心からこの割合以内なら首は動かさない
constexpr uint32_t kTrackMoveMs = 250;   // 1回の首の動きにかける時間
constexpr uint32_t kReturnAfterMs = 3000;  // 顔を見失ってから正面に戻るまで
bool tracking = true;

heian::Renderer face;
const int16_t* pcm = nullptr;  // 全セリフの PCM（2バイト境界にそろえたもの）

const VoiceClip* speaking = nullptr;  // 再生中のセリフ
uint32_t speakStartMs = 0;
float mouth = 0;
Preferences prefs;
int volume = 180;

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
  prefs.begin("heian");
  volume = prefs.getUChar("vol", 180);
  M5.Speaker.setVolume(volume);
}

void changeVolume(int d) {
  volume = constrain(volume + d, 0, 255);
  M5.Speaker.setVolume(volume);
  prefs.putUChar("vol", volume);
  Serial.printf("volume: %d\n", volume);
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

// 同じ category のセリフからランダムに1つ選んで話す（直前と同じものは避ける）
bool speakCategory(const char* category) {
  static int8_t lastPick[kVoiceClipCount];  // category の先頭の番号ごとに、前回選んだ番号＋1
  int idx[16], n = 0;
  for (int i = 0; i < kVoiceClipCount && n < 16; ++i)
    if (strcmp(kVoiceClips[i].category, category) == 0) idx[n++] = i;
  if (!n) { Serial.printf("speak: no clip for '%s'\n", category); return false; }
  int k = random(n);
  if (n > 1 && idx[k] == lastPick[idx[0]] - 1) k = (k + 1 + random(n - 1)) % n;
  lastPick[idx[0]] = idx[k] + 1;
  speak(idx[k]);
  return true;
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

// 接客の節で定義（シリアルの試しコマンドから呼ぶ）
void onEvent(const char* type, const char* verdict);
void onPet();
void greetNow();
void attractNow();
bool attendantHoldsNeck();

// シリアルの1文字コマンド（首の動作確認・接客の試し・ID書き換え・カメラ画像の取り出し）
void command(char c) {
  if (c == 'i' || c == 'I' || c == 'F' || c == 'X') { idCommand(c); return; }
  switch (c) {
    case 's': face_sensor::dumpSnapshot(); break;
    case 'e': neck::enable(); Serial.println("neck: torque on"); break;
    case 'd': neck::disable(); Serial.println("neck: torque off"); break;
    case 'c': neck::look(0, 0); break;
    case 'h': neck::look(neck::kHomePan, neck::kHomeTilt); break;
    case 'l': neck::look(30, 0); break;
    case 'r': neck::look(-30, 0); break;
    case 'u': neck::look(0, neck::kTiltMin, 1000); break;
    // 上下を 5° ずつ動かして、筐体に当たらない上限を探す
    case 'k': neck::look(neck::pan(), neck::tilt() - 5); Serial.printf("neck: tilt %+.0f\n", neck::tilt()); break;
    case 'j': neck::look(neck::pan(), neck::tilt() + 5); Serial.printf("neck: tilt %+.0f\n", neck::tilt()); break;
    case 'b': neck::play(neck::Motion::Bow); break;
    case 'n': neck::play(neck::Motion::Nod); break;
    case 'a': neck::play(neck::Motion::LookAround); break;
    case 't': tracking = !tracking; Serial.printf("tracking: %s\n", tracking ? "on" : "off"); if (!tracking) neck::look(neck::kHomePan, neck::kHomeTilt, 1000); break;
    case 'p': Serial.printf("neck pos: pan=%d tilt=%d\n", neck::readPos(neck::kPanId), neck::readPos(neck::kTiltId)); break;
    case '0': onEvent("game_start", ""); break;
    case '1': onEvent("game_end", "kokoro"); break;
    case '2': onEvent("game_end", "tashinami"); break;
    case '3': onEvent("game_end", "nashi"); break;
    case '7': onPet(); break;
    case '8': greetNow(); break;
    case '9': attractNow(); break;
    case '+': changeVolume(+20); break;
    case '-': changeVolume(-20); break;
  }
}

// JSON の1行から "key":"値" の値を取り出す（ライブラリを使わない簡単な読み方。値に " は無い前提）
bool jsonString(const char* line, const char* key, char* out, size_t cap) {
  char pat[24];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char* p = strstr(line, pat);
  if (!p) return false;
  p += strlen(pat);
  while (*p == ' ' || *p == ':') ++p;
  if (*p++ != '"') return false;
  size_t n = 0;
  while (*p && *p != '"' && n + 1 < cap) out[n++] = *p++;
  out[n] = 0;
  return true;
}

// シリアルを読む。{ で始まる行は PC からの知らせ、それ以外は1文字コマンド
void handleSerial() {
  static char line[320];
  static int len = -1;  // -1：知らせの行の途中ではない
  while (Serial.available()) {
    const char c = Serial.read();
    if (len >= 0) {
      if (c == '\n' || c == '\r') {
        line[len] = 0;
        len = -1;
        char type[24] = "", verdict[24] = "";
        if (jsonString(line, "type", type, sizeof(type))) {
          jsonString(line, "verdict", verdict, sizeof(verdict));
          onEvent(type, verdict);
        } else {
          Serial.printf("event: unreadable line: %s\n", line);
        }
      } else if (len < (int)sizeof(line) - 1) {
        line[len++] = c;
      }
      continue;
    }
    if (c == '{') { line[0] = c; len = 1; continue; }
    command(c);
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
//  - 顔を見失って kReturnAfterMs たったら、ゆっくり待機の向き（正面・少し上）に戻る
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

  if (!tracking || !neck::isEnabled() || neck::busy() || attendantHoldsNeck()) return;
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
    Serial.printf("track: face(%d,%d) ex=%+.2f ey=%+.2f -> pan %+.0f tilt %+.0f\n", f.x, f.y, ex, ey, neck::pan(), neck::tilt());
  } else if (!atHome && now - lostAt > kReturnAfterMs) {
    neck::look(neck::kHomePan, neck::kHomeTilt, 1500);
    atHome = true;
  }
}

// ======================= 接客 =======================
// ---- 設定 ----
constexpr uint32_t kAttractMinMs = 30000, kAttractMaxMs = 60000;   // 誰も遊んでいないときの呼び込みの間隔
constexpr uint32_t kWatchMinMs = 90000, kWatchMaxMs = 150000;      // 遊んでいる人がいるとき（VR の中の付き人の声と混ざらないよう少なめ）
constexpr uint32_t kPlayingTimeoutMs = 10 * 60000;  // game_start のあと game_end が来なくても、これで「遊んでいない」に戻す
constexpr uint32_t kFaceStableMs = 700;     // 顔がこれだけ続けて見えたら声をかける（見間違いで話さないため）
constexpr uint32_t kFaceGoneMs = 8000;      // 顔がこれだけ見えなかったら「立ち去った」とみなす（次に来た人には、また声をかける）
constexpr uint32_t kGreetCooldownMs = 45000;  // 声かけどうしの最短の間隔
constexpr uint32_t kPetCooldownMs = 4000;     // 撫でられての反応どうしの最短の間隔
constexpr int kPetMovePx = 30;                // 触れたまま指がこれだけ動いたら「撫でた」
constexpr uint32_t kPetHoldMs = 600;          // または、これだけ触れ続けたら
constexpr uint32_t kPrintWaitMs = 9000;       // 知らせが届いてから紙が出るまでの目安（「紙を取って」を言うのはこれ以降）
// プリンタの方向（度）。パン +で左（ロボットから見て）、チルト +で下。会場の置き場所に合わせて直す
constexpr float kPrinterPanDeg = 0, kPrinterTiltDeg = 15;

// ---- 状態 ----
enum class Ending { None, Verdict, Praise, WaitPrint, TurnToPrinter, Printed, Bow };
Ending ending = Ending::None;
uint32_t endingEventAt = 0, stageAt = 0;
bool playing = false;
uint32_t playingSince = 0;
uint32_t nextAttractAt = 0, lastGreetAt = 0, lastPetAt = 0, quietSince = 0;
bool faceVisible = false, greetedThisVisit = false;
uint32_t faceAppearedAt = 0, faceLastSeenAt = 0;

bool attendantHoldsNeck() { return ending != Ending::None; }
bool talking() { return M5.Speaker.isPlaying(); }

void scheduleAttract() {
  const uint32_t lo = playing ? kWatchMinMs : kAttractMinMs, hi = playing ? kWatchMaxMs : kAttractMaxMs;
  nextAttractAt = millis() + random(lo, hi);
}

void attractNow() {
  speakCategory(playing ? "watch" : "attract");
  if (!playing && !faceVisible && !neck::busy()) neck::play(neck::Motion::LookAround);  // 誰もいなければ見渡しながら
  scheduleAttract();
}

void greetNow() {
  speakCategory(playing ? "watch" : "greet");
  lastGreetAt = millis();
  greetedThisVisit = true;
  scheduleAttract();
}

void onPet() {
  const uint32_t now = millis();
  if (ending != Ending::None || talking() || now - lastPetAt < kPetCooldownMs) return;
  lastPetAt = now;
  Serial.println("pet");
  speakCategory("pet");
  scheduleAttract();
}

void onEvent(const char* type, const char* verdict) {
  Serial.printf("event: %s %s\n", type, verdict);
  if (strcmp(type, "game_start") == 0) {
    playing = true;
    playingSince = millis();
    scheduleAttract();
  } else if (strcmp(type, "game_end") == 0) {
    const bool known = !strcmp(verdict, "kokoro") || !strcmp(verdict, "tashinami") || !strcmp(verdict, "nashi");
    neck::stop();  // 見渡しの途中でも、やめて正面へ
    neck::look(neck::kHomePan, neck::kHomeTilt, 600);
    speakCategory(known ? verdict : "nashi");  // 割り込んで話す（speak が前のセリフを止める）
    ending = Ending::Verdict;
    endingEventAt = stageAt = millis();
    playing = false;
  }
}

// 画面を撫でたか（触れたまま指を動かす、または長めに触れる）
void checkPet() {
  static int16_t x0 = 0, y0 = 0;
  static uint32_t t0 = 0;
  static bool done = true;
  const auto t = M5.Touch.getDetail();
  if (t.wasPressed()) { x0 = t.x; y0 = t.y; t0 = millis(); done = false; }
  if (!t.isPressed() || done) return;
  if (abs(t.x - x0) + abs(t.y - y0) >= kPetMovePx || millis() - t0 >= kPetHoldMs) {
    done = true;
    onPet();
  }
}

// 結末の流れを1段ずつ進める（セリフが終わるのを待って次へ）
void updateEnding() {
  const uint32_t now = millis();
  auto next = [&](Ending e) { ending = e; stageAt = now; };
  switch (ending) {
    case Ending::None: return;
    case Ending::Verdict:
      if (!talking() && now - stageAt > 500) { speakCategory("praise"); next(Ending::Praise); }
      break;
    case Ending::Praise:
      if (!talking() && now - stageAt > 500) next(Ending::WaitPrint);
      break;
    case Ending::WaitPrint:
      if (now - endingEventAt >= kPrintWaitMs && now - stageAt > 300) {
        neck::look(kPrinterPanDeg, kPrinterTiltDeg, 800);
        next(Ending::TurnToPrinter);
      }
      break;
    case Ending::TurnToPrinter:
      if (now - stageAt > 900) { speakCategory("printed"); next(Ending::Printed); }
      break;
    case Ending::Printed:
      if (!talking() && now - stageAt > 500) { neck::play(neck::Motion::Bow); next(Ending::Bow); }
      break;
    case Ending::Bow:
      if (!neck::busy() && now - stageAt > 500) {
        neck::look(neck::kHomePan, neck::kHomeTilt, 1000);
        ending = Ending::None;
        lastGreetAt = now;          // 遊び終わった人にすぐ「ようこそ」と言わない
        greetedThisVisit = faceVisible;
        scheduleAttract();
        Serial.println("ending: done");
      }
      break;
  }
}

// 毎ループ：顔の出入り・声かけ・呼び込み・撫でる・結末
void updateAttendant() {
  const uint32_t now = millis();
  checkPet();
  updateEnding();

  // 顔の出入り
  const auto f = face_sensor::get();
  const bool seen = f.count > 0 && now - f.atMs < 1000;
  if (seen) {
    if (!faceVisible) { faceVisible = true; faceAppearedAt = now; }
    faceLastSeenAt = now;
  } else if (faceVisible && now - faceLastSeenAt > kFaceGoneMs) {
    faceVisible = false;
    greetedThisVisit = false;
  }

  if (playing && now - playingSince > kPlayingTimeoutMs) { playing = false; Serial.println("playing: timeout"); }
  if (talking()) { quietSince = now; return; }
  if (ending != Ending::None || now - quietSince < 1500) return;  // 話し終わってすぐは次を話さない

  if (faceVisible && seen && !greetedThisVisit && now - faceAppearedAt > kFaceStableMs &&
      now - lastGreetAt > kGreetCooldownMs) {
    greetNow();
  } else if (now >= nextAttractAt) {
    attractNow();
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
  randomSeed(esp_random());
  speakCategory("greet");
  lastGreetAt = millis();
  scheduleAttract();
}

void loop() {
  M5.update();
  reportStats();
  trackFace();

  handleSerial();
  neck::update();

  updateAttendant();

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
