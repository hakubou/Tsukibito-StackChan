// 畳台（たただい）：畳2枚を並べた台。A1 mini（180mm角）に収まるよう1枚ずつ印刷する
//  - 天面を下にして（ベッドに天面を付けて）印刷する。サポート不要、天面がきれいに仕上がる
//  - 中は空洞。CoreS3 用のモバイルバッテリーを入れる（内寸の高さ約25mm）
//  - 縁（へり）は本物の畳と同じく長辺側だけ。0.6mm 下げてあるので布や印刷した紙を貼る
//  - 2枚は M3 ボルト×2本で連結
part = "assembly";  // "mat_robot" | "mat_desk" | "assembly"

MAT_W = 120;  // 短辺（左右）
MAT_D = 150;  // 長辺（前後）
H = 28; WALL = 2; TOP = 3;
HERI_W = 9;  HERI_DEPTH = 0.6;   // 縁
ME_PITCH = 1.6; ME_W = 0.7; ME_DEPTH = 0.4;  // 畳目（短辺と平行な細い溝）
BOLT_D = 3.4; BOLT_Y = 45; BOLT_Z = 14;

// 胴台（dodai.scad の底寸法）と置き位置
DODAI_BW = 96; DODAI_BD = 74; DODAI_R = 8; DODAI_POS = [0, 8];
GROOVE_W = 2.8; GROOVE_DEPTH = 2;   // 胴の底をはめる溝
CABLE_D = 12;                       // 胴の後ろの配線穴

// 文机（fuzukue.scad）の脚の位置
LEG = 10; LEG_C = 42; DESK_POS = [0, -14]; LEG_POCKET = 1.5;

$fn = 48;

module rr(w, d, r, h) { linear_extrude(h) offset(r) square([w - 2 * r, d - 2 * r], center = true); }

// 天面が z=H にくる向きでモデル化（印刷時は裏返す）
module mat(kind) {
  difference() {
    translate([-MAT_W / 2, -MAT_D / 2, 0]) cube([MAT_W, MAT_D, H]);
    // 中の空洞（底は開放）
    translate([-MAT_W / 2 + WALL, -MAT_D / 2 + WALL, -1]) cube([MAT_W - 2 * WALL, MAT_D - 2 * WALL, H - TOP + 1]);
    // 縁（長辺＝左右の辺）
    for (s = [1, -1]) translate([s > 0 ? MAT_W / 2 - HERI_W : -MAT_W / 2 - 1, -MAT_D / 2 - 1, H - HERI_DEPTH])
      cube([HERI_W + 1, MAT_D + 2, 1]);
    // 畳目
    for (y = [-MAT_D / 2 + ME_PITCH : ME_PITCH : MAT_D / 2 - ME_PITCH])
      translate([-MAT_W / 2 + HERI_W, y - ME_W / 2, H - ME_DEPTH]) cube([MAT_W - 2 * HERI_W, ME_W, 1]);
    // 連結ボルト穴（ロボット側は左の壁、文机側は右の壁）
    for (y = [-BOLT_Y, BOLT_Y]) translate([kind == "robot" ? -MAT_W / 2 : MAT_W / 2, y, BOLT_Z])
      rotate([0, 90, 0]) cylinder(d = BOLT_D, h = WALL * 3, center = true);
    if (kind == "robot") {
      translate([DODAI_POS[0], DODAI_POS[1], H - GROOVE_DEPTH]) difference() {
        rr(DODAI_BW + 0.6, DODAI_BD + 0.6, DODAI_R, GROOVE_DEPTH + 1);
        translate([0, 0, -1]) rr(DODAI_BW + 0.6 - 2 * GROOVE_W, DODAI_BD + 0.6 - 2 * GROOVE_W, DODAI_R - GROOVE_W, GROOVE_DEPTH + 3);
      }
      translate([DODAI_POS[0], DODAI_POS[1] + DODAI_BD / 2 + CABLE_D / 2 + 2, H - TOP - 1]) cylinder(d = CABLE_D, h = TOP + 2);
    } else {
      for (sx = [1, -1], sy = [1, -1])
        translate([DESK_POS[0] + sx * LEG_C - (LEG + 0.6) / 2, DESK_POS[1] + sy * LEG_C - (LEG + 0.6) / 2, H - LEG_POCKET])
          cube([LEG + 0.6, LEG + 0.6, LEG_POCKET + 1]);
    }
  }
}

if (part == "mat_robot") translate([0, 0, H]) rotate([180, 0, 0]) mat("robot");
else if (part == "mat_desk") translate([0, 0, H]) rotate([180, 0, 0]) mat("desk");
else {
  color("darkkhaki") translate([MAT_W / 2, 0, 0]) mat("robot");
  color("khaki") translate([-MAT_W / 2, 0, 0]) mat("desk");
}
