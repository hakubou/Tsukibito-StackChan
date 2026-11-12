// 胴台（どうだい）：ｽﾀｯｸﾁｬﾝを載せる「胴」。雛人形の胴と同じく、布の装束を着せる芯になる
//  tube     … 胴（上下が開いた角錐台の筒）。サポート不要
//  plate    … 首台プレート：足のはめ込み穴・位置決めピン・襟（盤領）付き。胴の上にはめる
//  sleeve_r / sleeve_l … 袖の芯（ヒレ状の板）。胴の側面スリットに上から差し込み、プレートで押さえる
part = "assembly";  // "tube" | "plate" | "sleeve_r" | "sleeve_l" | "assembly"

H  = 45;            // 胴の高さ
BW = 96; BD = 74;   // 底の幅・奥行き
TW = 70; TD = 62;   // 上の幅・奥行き
R  = 8;             // 角の丸み
WALL = 2;

// ｽﾀｯｸﾁｬﾝの足（takao feet_scs0009）
FEET_W = 54.0; FEET_D = 50.0; FEET_R = 3; FEET_CLR = 0.4;
FEET_HOLE_DX = 16.1;  // 足裏の位置決め穴（Ø6.1）の左右位置
FEET_HOLE_DY = 4.6;   // 同・前後のずれ（+で後ろ）。足の向きが逆なら -4.6 にする
PEG_D = 5.6; PEG_H = 3;

PLATE_T = 2.5; POCKET = 1.5; LIP_H = 3; FIT = 0.3;
COLLAR_GAP = 3; COLLAR_T = 1.6; COLLAR_H_FRONT = 6; COLLAR_H_BACK = 10;
CABLE_W = 14;  // 襟の後ろの配線用切り欠き

SLOT_T = 2.4; FIN_T = 2.0; SLOT_Z0 = H - 14;

$fn = 48;

module rr(w, d, r, h) { linear_extrude(h) offset(r) square([w - 2 * r, d - 2 * r], center = true); }

module frustum(o = 0, ext = 0) {
  hull() {
    translate([0, 0, -ext]) rr(BW - 2 * o, BD - 2 * o, max(R - o, 1), 0.01);
    translate([0, 0, H + ext - 0.01]) rr(TW - 2 * o, TD - 2 * o, max(R - o, 1), 0.01);
  }
}

module tube() {
  difference() {
    frustum(0);
    frustum(WALL, 1);
    for (s = [1, -1]) translate([s * (TW / 2 - 6) + (s > 0 ? 0 : -30), -SLOT_T / 2, SLOT_Z0]) cube([30, SLOT_T, H]);
  }
}

module plate() {
  inner_w = TW - 2 * WALL - 2 * FIT;
  inner_d = TD - 2 * WALL - 2 * FIT;
  cw = FEET_W + 2 * COLLAR_GAP;
  cd = FEET_D + 2 * COLLAR_GAP;
  difference() {
    union() {
      rr(TW, TD, R, PLATE_T);
      // 胴の内側にはまる縁（袖のツメの位置は逃がす）
      difference() {
        translate([0, 0, -LIP_H]) rr(inner_w, inner_d, R - 2, LIP_H);
        translate([0, 0, -LIP_H - 1]) rr(inner_w - 3.2, inner_d - 3.2, R - 3.6, LIP_H + 2);
        translate([0, 0, -LIP_H / 2]) cube([TW + 2, SLOT_T + 1, LIP_H + 2], center = true);
      }
      // 襟（盤領）：後ろが高く前が低い
      difference() {
        translate([0, 0, PLATE_T]) rr(cw + 2 * COLLAR_T, cd + 2 * COLLAR_T, FEET_R + COLLAR_GAP + COLLAR_T, COLLAR_H_BACK);
        translate([0, 0, PLATE_T - 1]) rr(cw, cd, FEET_R + COLLAR_GAP, COLLAR_H_BACK + 2);
        // 前を低くする斜めカット
        translate([0, 0, PLATE_T + COLLAR_H_FRONT])
          rotate([atan((COLLAR_H_BACK - COLLAR_H_FRONT) / (cd + 2 * COLLAR_T)), 0, 0])
          translate([-50, -50, 0]) cube([100, 100, 30]);
        // 配線の切り欠き（後ろ）
        translate([-CABLE_W / 2, cd / 2 - 1, PLATE_T - 1]) cube([CABLE_W, COLLAR_T + 3, COLLAR_H_BACK + 2]);
      }
    }
    // 足のはめ込み穴
    translate([0, 0, PLATE_T - POCKET]) rr(FEET_W + 2 * FEET_CLR, FEET_D + 2 * FEET_CLR, FEET_R, POCKET + 1);
  }
  // 位置決めピン（ポケットの底から生やす。先端は面取り）
  for (s = [1, -1]) translate([s * FEET_HOLE_DX, FEET_HOLE_DY, PLATE_T - POCKET])
    cylinder(d1 = PEG_D, d2 = PEG_D - 1.2, h = PEG_H);
}

// 胴の外壁のx位置（高さz）
function xw(z) = BW / 2 - (BW - TW) / 2 * z / H;

module sleeve2d() {
  union() {
    polygon([[xw(H), H], [52, H - 1], [66, 36], [74, 22], [76, 10], [72, 4], [xw(4), 4]]);
    translate([TW / 2 - 6 + 0.3, SLOT_Z0 + 0.3]) square([xw(SLOT_Z0) - (TW / 2 - 6) + 1, H - SLOT_Z0 - 0.3]);
  }
}

module sleeve_flat() { linear_extrude(FIN_T) sleeve2d(); }

if (part == "tube") tube();
else if (part == "plate") translate([0, 0, LIP_H]) plate();
else if (part == "sleeve_r") sleeve_flat();
else if (part == "sleeve_l") mirror([1, 0, 0]) sleeve_flat();
else {
  color("tan") tube();
  color("peru") translate([0, 0, H]) plate();
  for (m = [0, 1]) mirror([m, 0, 0]) color("burlywood") translate([0, FIN_T / 2, 0]) rotate([90, 0, 0]) sleeve_flat();
}
