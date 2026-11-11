// 平安顔（引目鉤鼻・置眉・紅の口・頬紅）の M5Stack-Avatar 用パーツ
//  - 画面 320x240、16bit カラーで描く（init(16) で起動すること）
//  - 配置は docs/decoration.md 2-2 の設計値
#pragma once
#include <Avatar.h>

namespace heian {

using namespace m5avatar;

// 色（RGB565）
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
constexpr uint16_t kGofun   = rgb(0xF3, 0xEE, 0xE3);  // 胡粉白（背景）
constexpr uint16_t kSumi    = rgb(0x2B, 0x2B, 0x2B);  // 墨（目）
constexpr uint16_t kMayu    = rgb(0x3A, 0x3A, 0x3A);  // 置眉
constexpr uint16_t kMayuSoft= rgb(0xA8, 0xA4, 0x9C);  // 置眉のぼかし
constexpr uint16_t kNose    = rgb(0x8A, 0x7F, 0x75);  // 鉤鼻の線
constexpr uint16_t kBeni    = rgb(0xB8, 0x34, 0x3F);  // 紅（口）
constexpr uint16_t kBeniDark= rgb(0x5A, 0x10, 0x18);  // 口の中
constexpr uint16_t kHoho    = rgb(0xEF, 0xC3, 0xC0);  // 頬紅
constexpr uint16_t kBalloon = rgb(0xFF, 0xFD, 0xF5);  // 吹き出しの地

// 2次ベジェ曲線を太線で描く（引目用）
inline void drawCurve(M5Canvas* c, float x0, float y0, float cx, float cy,
                      float x1, float y1, float r, uint16_t color) {
  constexpr int kSteps = 12;
  float px = x0, py = y0;
  for (int i = 1; i <= kSteps; ++i) {
    float t = (float)i / kSteps, u = 1 - t;
    float x = u * u * x0 + 2 * u * t * cx + t * t * x1;
    float y = u * u * y0 + 2 * u * t * cy + t * t * y1;
    c->drawWideLine(px, py, x, y, r, color);
    px = x;
    py = y;
  }
}

// 引目（細い線の目）＋頬紅
class Eye final : public Drawable {
  bool isLeft_;  // 画面右側に描く目なら true

 public:
  explicit Eye(bool isLeft) : isLeft_(isLeft) {}

  void draw(M5Canvas* c, BoundingRect rect, DrawContext* ctx) override {
    const int cx = rect.getLeft();
    const int cy = rect.getTop();
    const float open = isLeft_ ? ctx->getLeftEyeOpenRatio() : ctx->getRightEyeOpenRatio();
    const Gaze gaze = isLeft_ ? ctx->getLeftGaze() : ctx->getRightGaze();
    const Expression exp = ctx->getExpression();
    const int outer = isLeft_ ? 1 : -1;  // 目尻の向き

    // 頬紅（目の下 34px、外側へ 18px）
    c->fillEllipse(cx + outer * 18, cy + 34, 22, 12, kHoho);

    constexpr float kHalf = 22;  // 目の半幅
    float yIn = cy, yOut = cy;   // 目頭・目尻の高さ
    float bend = 4;              // +で下にふくらむ
    float r = 1.6f;
    switch (exp) {
      case Expression::Happy:  bend = -9; break;              // 笑み（∩）
      case Expression::Sad:    yOut += 5; yIn -= 2; break;     // 目尻が下がる
      case Expression::Angry:  yIn += 4; yOut -= 3; bend = 1; break;
      case Expression::Sleepy: yIn += 3; yOut += 3; bend = 1; r = 1.2f; break;
      case Expression::Doubt:  if (isLeft_) { yIn -= 2; yOut -= 2; } bend = 2; break;
      default: break;
    }

    const float xIn = cx - outer * kHalf, xOut = cx + outer * kHalf;
    if (open < 0.5f) {  // まばたき中は細い直線
      c->drawWideLine(xIn, (yIn + yOut) / 2 + 2, xOut, (yIn + yOut) / 2 + 2, 1.0f, kSumi);
      return;
    }
    drawCurve(c, xIn, yIn, cx, cy + bend * 2, xOut, yOut, r, kSumi);

    // 引目の中の小さな瞳（視線に合わせてわずかに動く）
    if (exp != Expression::Happy && exp != Expression::Sleepy) {
      const float gx = gaze.getHorizontal() * 4;
      const float midY = (yIn + yOut) / 2 + bend + 1;
      c->fillEllipse(cx + gx, midY, 3, 2, kSumi);
    }
  }
};

// 置眉（額の高い位置の丸い眉）
class Mayu final : public Drawable {
  bool isLeft_;

 public:
  explicit Mayu(bool isLeft) : isLeft_(isLeft) {}

  void draw(M5Canvas* c, BoundingRect rect, DrawContext* ctx) override {
    int cx = rect.getLeft();
    int cy = rect.getTop();
    switch (ctx->getExpression()) {
      case Expression::Happy:  cy -= 3; break;
      case Expression::Sad:    cy -= 6; cx += isLeft_ ? -3 : 3; break;  // 困り眉風に内へ寄せて上げる
      case Expression::Angry:  cy += 4; cx += isLeft_ ? -4 : 4; break;
      case Expression::Sleepy: cy += 2; break;
      case Expression::Doubt:  if (isLeft_) cy -= 6; break;
      default: break;
    }
    c->fillEllipse(cx, cy, 17, 11, kMayuSoft);
    c->fillEllipse(cx, cy, 12, 8, kMayu);
  }
};

// 紅の小さな口＋鉤鼻
class Kuchi final : public Drawable {
 public:
  void draw(M5Canvas* c, BoundingRect rect, DrawContext* ctx) override {
    const int cx = rect.getLeft();
    const int cy = rect.getTop();

    // 鉤鼻（口の 34px 上に「く」の字）
    const int ny = cy - 34;
    c->drawWideLine(cx + 2, ny - 7, cx - 3, ny + 1, 0.9f, kNose);
    c->drawWideLine(cx - 3, ny + 1, cx + 3, ny + 3, 0.9f, kNose);

    const float open = ctx->getMouthOpenRatio();
    const Expression exp = ctx->getExpression();
    float rx = 6, ry = 3.5f;
    if (exp == Expression::Happy) { rx = 8; ry = 3; }
    if (exp == Expression::Sad)   { rx = 5; }
    ry += 7 * open;
    rx -= 1.5f * open;
    c->fillEllipse(cx, cy, rx, ry, kBeni);
    if (open > 0.15f) {
      c->fillEllipse(cx, cy + 0.5f, rx - 2.5f, ry - 2.5f, kBeniDark);
    }
  }
};

// 平安顔の Face を作る（スプライトは PSRAM に確保）
inline Face* createFace() {
  auto* sprite = new M5Canvas(&M5.Display);
  sprite->setPsram(true);
  auto* tmp = new M5Canvas(&M5.Display);
  // BoundingRect(top=y, left=x) はパーツの中心座標
  return new Face(new Kuchi(), new BoundingRect(182, 160),
                  new Eye(false), new BoundingRect(118, 110),
                  new Eye(true), new BoundingRect(118, 210),
                  new Mayu(false), new BoundingRect(58, 110),
                  new Mayu(true), new BoundingRect(58, 210),
                  new BoundingRect(0, 0, 320, 240), sprite, tmp);
}

inline ColorPalette createPalette() {
  ColorPalette p;
  p.set(COLOR_PRIMARY, kSumi);
  p.set(COLOR_SECONDARY, kBeni);
  p.set(COLOR_BACKGROUND, kGofun);
  p.set(COLOR_BALLOON_FOREGROUND, kSumi);
  p.set(COLOR_BALLOON_BACKGROUND, kBalloon);
  return p;
}

}  // namespace heian
