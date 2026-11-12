// 文机（ふづくえ）：感熱プリンタ BluwieBroka S1 のカバー
//  body … 四隅に脚のある箱。S1 にかぶせる（底なし）。上下開放の筒なのでサポート不要
//  top  … 天板。排紙スリット付き。ネオジム磁石（φ6×3）4個で本体に付く → 外せば紙交換できる
//  ★ S1 到着後に S1_W/S1_D/S1_H と SLOT_X/SLOT_Y、背面窓の位置を実測値に直すこと
part = "assembly";  // "body" | "top" | "assembly"

S1_W = 84; S1_D = 84; S1_H = 43;  // 公称値
CLR = 1;
WALL = 2;
IN_W = S1_W + 2 * CLR; IN_D = S1_D + 2 * CLR;
BODY_H = S1_H + 2;
OUT_W = IN_W + 2 * WALL; OUT_D = IN_D + 2 * WALL;   // 90 × 90
LEG = 10; LEG_OUT = 2;                              // 脚は外へ 2mm 張り出す
LEG_C = OUT_W / 2 + LEG_OUT - LEG / 2;              // 脚の中心（42）
APRON_LINE_Z = BODY_H - 9;                          // 幕板の線

MAG_D = 6.3; MAG_H = 3.2;

TOP_W = 114; TOP_D = 100; TOP_T = 4; TOP_R = 2;
FUDEGAESHI_H = 2.5; FUDEGAESHI_W = 5;  // 筆返し（天板の両端の反り）
SLOT_W = 64; SLOT_D = 10;   // 排紙スリット（幅は紙57mm＋余裕）
SLOT_X = 0; SLOT_Y = -22;   // スリット中心（-Y が手前）★実測で調整
TEAR_CHAMFER = 3;           // 手前側の面取り（紙を手前に引いて切りやすく）

BACK_WIN_W = 44; BACK_WIN_H = 22; BACK_WIN_Z = 4;  // 背面窓（USB・電源ボタン・ランプ）★実測で調整

$fn = 40;

module desk_body() {
  difference() {
    union() {
      translate([-OUT_W / 2, -OUT_D / 2, 0]) cube([OUT_W, OUT_D, BODY_H]);
      for (sx = [1, -1], sy = [1, -1]) translate([sx * LEG_C - LEG / 2, sy * LEG_C - LEG / 2, 0]) cube([LEG, LEG, BODY_H]);
    }
    translate([-IN_W / 2, -IN_D / 2, -1]) cube([IN_W, IN_D, BODY_H + 2]);
    // 幕板の線（細い溝）
    for (r = [0, 90, 180, 270]) rotate([0, 0, r])
      translate([-(LEG_C - LEG / 2), -OUT_D / 2 - 1, APRON_LINE_Z]) cube([2 * (LEG_C - LEG / 2), 1.6, 0.8]);
    // 磁石穴（脚の上面）
    for (sx = [1, -1], sy = [1, -1]) translate([sx * LEG_C, sy * LEG_C, BODY_H - MAG_H]) cylinder(d = MAG_D, h = MAG_H + 1);
    // 背面窓（+Y が奥＝スタッフ側）
    translate([-BACK_WIN_W / 2, OUT_D / 2 - WALL - 1, BACK_WIN_Z]) cube([BACK_WIN_W, WALL + 2, BACK_WIN_H]);
  }
}

module desk_top() {
  difference() {
    union() {
      linear_extrude(TOP_T) offset(TOP_R) square([TOP_W - 2 * TOP_R, TOP_D - 2 * TOP_R], center = true);
      // 筆返し
      for (s = [1, -1]) translate([s * (TOP_W / 2 - FUDEGAESHI_W / 2), 0, TOP_T])
        scale([FUDEGAESHI_W / 2, TOP_D / 2, FUDEGAESHI_H]) rotate([90, 0, 0]) cylinder(r = 1, h = 2, center = true, $fn = 32);
    }
    // 排紙スリット＋手前の面取り
    translate([SLOT_X - SLOT_W / 2, SLOT_Y - SLOT_D / 2, -1]) cube([SLOT_W, SLOT_D, TOP_T + 2]);
    translate([SLOT_X - SLOT_W / 2, SLOT_Y - SLOT_D / 2, TOP_T]) rotate([45, 0, 0]) cube([SLOT_W, TEAR_CHAMFER * 1.42, TEAR_CHAMFER * 1.42], center = false);
    // 磁石穴（裏面）
    for (sx = [1, -1], sy = [1, -1]) translate([sx * LEG_C, sy * LEG_C, -1]) cylinder(d = MAG_D, h = MAG_H + 1);
  }
}

if (part == "body") desk_body();
else if (part == "top") desk_top();
else {
  color([0.12, 0.1, 0.09]) desk_body();
  color([0.15, 0.12, 0.1]) translate([0, 0, BODY_H + 0.2]) desk_top();
}
