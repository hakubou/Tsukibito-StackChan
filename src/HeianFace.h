// 平安顔（引目鉤鼻・置眉・紅の口・頬紅）の描画
//  - M5Stack-Avatar は使わず、画面を横長の帯（320x24）に分けて内蔵RAMで描き、DMAで転送する
//    （全画面の16bitスプライトをPSRAMに作る方式は毎秒8コマ程度しか出なかったため）
//  - まばたき・呼吸・視線のゆらぎは、描画タスクの中で自動で動かす
//  - パーツの配置は docs/decoration.md 2-2 の設計値。tools/render_face.py も同じ値で描く
#pragma once
#include <M5Unified.h>

namespace heian {

enum class Expr { Neutral, Happy, Sad, Doubt, Sleepy, Angry };

// 色（RGB565）
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
constexpr uint16_t kGofun    = rgb(0xF3, 0xEE, 0xE3);  // 胡粉白（背景）
constexpr uint16_t kSumi     = rgb(0x2B, 0x2B, 0x2B);  // 墨（目）
constexpr uint16_t kMayu     = rgb(0x3A, 0x3A, 0x3A);  // 置眉
constexpr uint16_t kMayuSoft = rgb(0xA8, 0xA4, 0x9C);  // 置眉のぼかし
constexpr uint16_t kNose     = rgb(0x8A, 0x7F, 0x75);  // 鉤鼻の線
constexpr uint16_t kBeni     = rgb(0xB8, 0x34, 0x3F);  // 紅（口）
constexpr uint16_t kBeniDark = rgb(0x5A, 0x10, 0x18);  // 口の中
constexpr uint16_t kHoho     = rgb(0xEF, 0xC3, 0xC0);  // 頬紅

// パーツの中心座標
constexpr int kEyeY = 118, kEyeRX = 110, kEyeLX = 210;  // R＝画面左、L＝画面右
constexpr int kMayuY = 58;
constexpr int kMouthX = 160, kMouthY = 182;

// 描画に使う状態（1フレーム分のスナップショット）
struct State {
  Expr expr = Expr::Neutral;
  float mouthOpen = 0;  // 0〜1
  float eyeOpen = 1;    // 0.5 未満でまばたき中
  float gazeX = 0;      // -1（画面左）〜 1（画面右）
  float gazeY = 0;      // -1（上）〜 1（下）
  float breath = 0;     // -1〜1（全体が上下に少し動く）
};

// ---- パーツの描画（dy：帯の上端の y。帯の中の座標に直すために引く） ----

inline void curve(M5Canvas& c, int dy, float x0, float y0, float cx, float cy,
                  float x1, float y1, float r, uint16_t color) {
  constexpr int kSteps = 12;
  float px = x0, py = y0;
  for (int i = 1; i <= kSteps; ++i) {
    const float t = (float)i / kSteps, u = 1 - t;
    const float x = u * u * x0 + 2 * u * t * cx + t * t * x1;
    const float y = u * u * y0 + 2 * u * t * cy + t * t * y1;
    c.drawWideLine(px, py - dy, x, y - dy, r, color);
    px = x;
    py = y;
  }
}

inline void eye(M5Canvas& c, int dy, int cx, int cy, bool isLeft, const State& s) {
  const int outer = isLeft ? 1 : -1;
  c.fillEllipse(cx + outer * 18, cy + 34 - dy, 22, 12, kHoho);  // 頬紅

  constexpr float kHalf = 22;
  float yIn = cy, yOut = cy, bend = 4, r = 1.6f;
  switch (s.expr) {
    case Expr::Happy:  bend = -9; break;
    case Expr::Sad:    yOut += 5; yIn -= 2; break;
    case Expr::Angry:  yIn += 4; yOut -= 3; bend = 1; break;
    case Expr::Sleepy: yIn += 3; yOut += 3; bend = 1; r = 1.2f; break;
    case Expr::Doubt:  if (isLeft) { yIn -= 2; yOut -= 2; } bend = 2; break;
    default: break;
  }
  const float xIn = cx - outer * kHalf, xOut = cx + outer * kHalf;
  if (s.eyeOpen < 0.5f) {
    const float y = (yIn + yOut) / 2 + 2 - dy;
    c.drawWideLine(xIn, y, xOut, y, 1.0f, kSumi);
    return;
  }
  curve(c, dy, xIn, yIn, cx, cy + bend * 2, xOut, yOut, r, kSumi);
  if (s.expr != Expr::Happy && s.expr != Expr::Sleepy) {  // 引目の中の小さな瞳
    const float px = cx + s.gazeX * 9;
    const float py = (yIn + yOut) / 2 + bend + 1 + s.gazeY * 1.5f;
    c.fillEllipse(px, py - dy, 3, 2, kSumi);
  }
}

inline void mayu(M5Canvas& c, int dy, int cx, int cy, bool isLeft, const State& s) {
  switch (s.expr) {
    case Expr::Happy:  cy -= 3; break;
    case Expr::Sad:    cy -= 6; cx += isLeft ? -3 : 3; break;
    case Expr::Angry:  cy += 4; cx += isLeft ? -4 : 4; break;
    case Expr::Sleepy: cy += 2; break;
    case Expr::Doubt:  if (isLeft) cy -= 6; break;
    default: break;
  }
  c.fillEllipse(cx, cy - dy, 17, 11, kMayuSoft);
  c.fillEllipse(cx, cy - dy, 12, 8, kMayu);
}

inline void kuchi(M5Canvas& c, int dy, int cx, int cy, const State& s) {
  const int ny = cy - 34 - dy;  // 鉤鼻
  c.drawWideLine(cx + 2, ny - 7, cx - 3, ny + 1, 0.9f, kNose);
  c.drawWideLine(cx - 3, ny + 1, cx + 3, ny + 3, 0.9f, kNose);

  float rx = 6, ry = 3.5f;
  if (s.expr == Expr::Happy) { rx = 8; ry = 3; }
  if (s.expr == Expr::Sad)   { rx = 5; }
  ry += 7 * s.mouthOpen;
  rx -= 1.5f * s.mouthOpen;
  c.fillEllipse(cx, cy - dy, rx, ry, kBeni);
  if (s.mouthOpen > 0.15f) c.fillEllipse(cx, cy + 0.5f - dy, rx - 2.5f, ry - 2.5f, kBeniDark);
}

// 1フレーム描く（帯ごとに描いて DMA で送る。2枚の帯を交互に使い、送信中に次を描く）
class Renderer {
 public:
  static constexpr int kW = 320, kH = 240, kStripH = 24;

  void begin() {
    for (auto& s : strip_) {
      s.setColorDepth(16);
      s.setPsram(false);
      s.createSprite(kW, kStripH);
    }
    lock_ = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(taskEntry, "heianFace", 4096, this, 1, nullptr, 1);
  }

  // ---- 外から変える値 ----
  void setExpr(Expr e) { with([&] { target_.expr = e; }); }
  void setMouthOpen(float v) { with([&] { target_.mouthOpen = v; }); }
  // 視線を固定する（顔を見つけたとき）。follow=false で自動のきょろきょろに戻す
  void lookAt(float x, float y, bool follow = true) {
    with([&] { lookX_ = x; lookY_ = y; follow_ = follow; });
  }

  volatile uint32_t frameCount = 0;  // 描いたフレーム数（速度計測用）

 private:
  M5Canvas strip_[2];
  SemaphoreHandle_t lock_ = nullptr;
  State target_;
  float lookX_ = 0, lookY_ = 0;
  bool follow_ = false;

  template <class F> void with(F f) {
    xSemaphoreTake(lock_, portMAX_DELAY);
    f();
    xSemaphoreGive(lock_);
  }

  static void taskEntry(void* p) { static_cast<Renderer*>(p)->loop(); }

  void loop() {
    uint32_t nextBlink = millis() + 2500, blinkEnd = 0, nextSaccade = 0;
    float autoX = 0, autoY = 0, gx = 0, gy = 0;
    for (;;) {
      const uint32_t now = millis();
      State s;
      bool follow;
      float lx, ly;
      with([&] { s = target_; follow = follow_; lx = lookX_; ly = lookY_; });

      // まばたき（2.5〜4.5秒おきに 0.15秒）
      if (now >= nextBlink) { blinkEnd = now + 150; nextBlink = now + 2500 + random(2000); }
      s.eyeOpen = now < blinkEnd ? 0 : 1;
      // 視線：顔を追っているときはその方向、そうでなければときどき別の場所を見る
      if (now >= nextSaccade) {
        autoX = (random(201) - 100) / 250.0f;
        autoY = (random(201) - 100) / 300.0f;
        nextSaccade = now + 800 + random(2000);
      }
      const float tx = follow ? lx : autoX, ty = follow ? ly : autoY;
      gx += (tx - gx) * 0.35f;
      gy += (ty - gy) * 0.35f;
      s.gazeX = gx;
      s.gazeY = gy;
      // 呼吸（4秒周期で 2px 上下）
      s.breath = sinf(now / 4000.0f * 2 * PI);

      drawFrame(s);
      frameCount++;
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  void drawFrame(const State& s) {
    const int b = (int)roundf(s.breath * 2);
    M5.Display.startWrite();
    for (int y0 = 0, i = 0; y0 < kH; y0 += kStripH, i ^= 1) {
      M5Canvas& c = strip_[i];
      const int dy = y0 - b;  // 呼吸のぶん全体をずらす
      c.fillScreen(kGofun);
      kuchi(c, dy, kMouthX, kMouthY, s);
      eye(c, dy, kEyeRX, kEyeY, false, s);
      eye(c, dy, kEyeLX, kEyeY, true, s);
      mayu(c, dy, kEyeRX, kMayuY, false, s);
      mayu(c, dy, kEyeLX, kMayuY, true, s);
      c.pushSprite(&M5.Display, 0, y0);  // startWrite 中なので DMA で送ってすぐ戻る
    }
    M5.Display.endWrite();
  }
};

}  // namespace heian
