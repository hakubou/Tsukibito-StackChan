// 全体の組み立て確認用（印刷しない）。ｽﾀｯｸﾁｬﾝ本体は箱で代用
use <tatami.scad>
use <dodai.scad>
use <fuzukue.scad>
use <eboshi.scad>
use <byobu.scad>

MAT_W = 120; MAT_H = 28; DODAI_H = 45; PLATE_T = 2.5;
robot_xy = [MAT_W / 2, 8];
desk_xy  = [-MAT_W / 2, -14];

color("darkkhaki") translate([MAT_W / 2, 0, 0]) mat("robot");
color("khaki") translate([-MAT_W / 2, 0, 0]) mat("desk");

translate([robot_xy[0], robot_xy[1], MAT_H]) {
  color("darkolivegreen") tube();
  color("indigo") translate([0, 0, DODAI_H]) plate();
  for (m = [0, 1]) mirror([m, 0, 0]) color("darkolivegreen") translate([0, 1, 0]) rotate([90, 0, 0]) sleeve_flat();
  // ｽﾀｯｸﾁｬﾝ（代用の箱）：足 → 頭（CoreS3＋シェル）
  translate([0, 0, DODAI_H + PLATE_T - 1.5]) {
    color("gray") translate([-27, -25, 0]) cube([54, 50, 10]);
    color("white") translate([-27.3, -22, 22]) cube([54.6, 44, 55]);
    color([0.95, 0.93, 0.89]) translate([-24, -22.5, 26]) cube([48, 1, 40]);   // 画面
    translate([0, -20 + 15.7 - 7.7, 22 + 55 - 1]) color("black") shell();
  }
}

translate([desk_xy[0], desk_xy[1], MAT_H]) {
  color([0.95, 0.95, 0.95]) translate([-42, -42, 0]) cube([84, 84, 43]);   // S1
  color([0.12, 0.1, 0.09]) desk_body();
  color([0.15, 0.12, 0.1]) translate([0, 0, 45.2]) desk_top();
  color("white") translate([-28.5, -14 - 22 + 14, 49]) rotate([-8, 0, 0]) cube([57, 0.3, 110]);  // 短冊
}

translate([0, 105, 0]) for (i = [0 : 3]) translate([(i - 1.5) * 60 * cos(25), (i % 2) * 60 * sin(25), 0])
  rotate([0, 0, (i % 2 == 0) ? 25 : -25]) panel();
