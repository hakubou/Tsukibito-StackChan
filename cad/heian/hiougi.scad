// 小物：檜扇（ひおうぎ、男性用は白木・飾りなし）と硯箱（すずりばこ）
part = "plate";  // "hiougi" | "suzuribako" | "plate"

$fn = 48;

// 少し開いた檜扇（扇形、板の筋入り）。袖の前に貼る
module hiougi() {
  R = 32; ANG = 50; T = 1.4;
  difference() {
    linear_extrude(T) intersection() {
      circle(R);
      polygon([[0, 0], [R * 2 * sin(-ANG / 2), R * 2 * cos(ANG / 2)], [R * 2 * sin(ANG / 2), R * 2 * cos(ANG / 2)]]);
    }
    for (a = [-ANG / 2 + 5 : 5 : ANG / 2 - 5]) rotate([0, 0, a]) translate([-0.3, 6, T - 0.4]) cube([0.6, R, 1]);
  }
  cylinder(d = 4, h = T + 0.8);  // 要（かなめ）
}

module suzuribako() {
  W = 32; D = 24; H = 6;
  difference() {
    translate([-W / 2, -D / 2, 0]) cube([W, D, H]);
    translate([-W / 2 + 1.2, -D / 2 + 1.2, 1.2]) cube([W - 2.4, D - 2.4, H]);
  }
  translate([-6, -4, 0]) cube([12, 8, 2.5]);  // 硯
  // 蓋（ずらして載せる）
  translate([W / 2 + 4, 0, 0]) difference() {
    translate([-W / 2 - 0.6, -D / 2 - 0.6, 0]) cube([W + 1.2, D + 1.2, 2]);
    translate([-W / 2 + 3, -D / 2 + 3, 1.6]) difference() {   // 蒔絵の金線を描く溝
      cube([W - 6, D - 6, 1]);
      translate([0.6, 0.6, -1]) cube([W - 7.2, D - 7.2, 3]);
    }
  }
}

if (part == "hiougi") hiougi();
else if (part == "suzuribako") suzuribako();
else { hiougi(); translate([40, 10, 0]) suzuribako(); }
