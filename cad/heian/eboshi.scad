// 立烏帽子（たてえぼし）
//  - 猫耳HATのクリップ部分（耳を削除）に、烏帽子本体を瞬間接着剤で貼る2部品構成
//  - 本体は中空。目標重量 15g 以下（SCS0009 の負担を減らすため）
//  - Bambu Studio の「ファジースキン」を外壁にかけると、烏帽子の「さび（シワ）」風になる
part = "assembly";  // "shell" | "clip" | "assembly"

H    = 70;    // 高さ
W    = 44;    // 幅（左右）
D    = 30;    // 奥行き
LEAN = 8;     // 頂部を後ろへ傾ける量
T    = 0.9;   // 壁厚
FLOOR = 1.2;  // 底板の厚さ（クリップに接着する面）
DENT_R = 12;  // 招（まねき：前面上部のくぼみ）の球の半径
DENT_DEPTH = 2;
CORD_D = 2.2; // 掛緒（組紐）を通す穴

HAT_STL = "../reference/takao/stackchan_takao_hat_cat_CoreS3.stl";
CLIP_TOP_Z = 59.4;           // 猫耳HATのバー上面の高さ（STL座標）
CLIP_CENTER_Y = 15.7;        // バーの奥行き中心（STL座標）

$fn = 72;

module oval(w, d) { scale([w / 2, d / 2, 1]) cylinder(r = 1, h = 0.01); }

module eboshi_body(o = 0) {
  hull() {
    oval(W - 2 * o, D - 2 * o);
    translate([0, LEAN * 0.35, H * 0.55]) oval(W - 4 - 2 * o, D - 3 - 2 * o);
    translate([0, LEAN, H - 11]) scale([(W - 16) / 2 - o, (D - 10) / 2 - o, 11 - o]) sphere(1);
  }
}

// 前面上部のくぼみ位置
dent_z = H * 0.74;
dent_y = -(D - 8) / 2 + LEAN * 0.6 - DENT_R + DENT_DEPTH;

module shell() {
  difference() {
    difference() {
      eboshi_body(0);
      translate([0, dent_y, dent_z]) sphere(DENT_R);
    }
    difference() {
      translate([0, 0, FLOOR]) eboshi_body(T);
      translate([0, dent_y, dent_z]) sphere(DENT_R + T);
    }
    // 掛緒の穴（左右）
    translate([0, 0, 7]) rotate([0, 90, 0]) cylinder(d = CORD_D, h = W + 10, center = true, $fn = 24);
  }
}

module clip() {
  intersection() {
    import(HAT_STL);
    translate([-30, 0, 40]) cube([60, 40, CLIP_TOP_Z + 0.1 - 40]);
  }
}

if (part == "shell") shell();
else if (part == "clip") translate([0, -CLIP_CENTER_Y, -48]) clip();
else {
  color("dimgray") clip();
  color("black") translate([0, CLIP_CENTER_Y, CLIP_TOP_Z]) shell();
}
