// 屏風（びょうぶ）の框（わく）：印刷するのは上下の横木だけ。縦は 5mm 角の角棒（100円ショップ等）
//  1扇 = 横木2本＋角棒2本。4扇で四曲一隻。扇どうしの蝶番は布テープ（本物の屏風も紙の蝶番）
//  完成高さ = ROD_LEN + 2*(RAIL_H - SOCKET_DEPTH)。角棒 246mm で 250mm
part = "plate";  // "rail" | "plate"（横木8本をまとめて1枚に）| "assembly"

PANEL_W = 60;
RAIL_H = 8; RAIL_T = 7;
ROD = 5; ROD_CLR = 0.3; SOCKET_DEPTH = 6;
ROD_INSET = 4;     // 横木の端から角棒の中心まで
ROD_LEN = 246;

module rail() {
  difference() {
    translate([-PANEL_W / 2, -RAIL_T / 2, 0]) cube([PANEL_W, RAIL_T, RAIL_H]);
    for (s = [1, -1]) translate([s * (PANEL_W / 2 - ROD_INSET) - (ROD + ROD_CLR) / 2, -(ROD + ROD_CLR) / 2, RAIL_H - SOCKET_DEPTH])
      cube([ROD + ROD_CLR, ROD + ROD_CLR, SOCKET_DEPTH + 1]);
  }
}

module panel() {
  rail();
  translate([0, 0, 2 * RAIL_H + ROD_LEN - 2 * SOCKET_DEPTH]) mirror([0, 0, 1]) rail();
  for (s = [1, -1]) translate([s * (PANEL_W / 2 - ROD_INSET) - ROD / 2, -ROD / 2, RAIL_H - SOCKET_DEPTH]) cube([ROD, ROD, ROD_LEN]);
  color("ivory", 0.9) translate([-PANEL_W / 2, -RAIL_T / 2 - 0.5, RAIL_H]) cube([PANEL_W, 0.3, ROD_LEN - 2 * SOCKET_DEPTH]);
}

if (part == "rail") rail();
else if (part == "plate") for (i = [0 : 7]) translate([0, i * (RAIL_T + 4), 0]) rail();
else {
  // ジグザグに開いた状態
  for (i = [0 : 3]) translate([(i - 1.5) * PANEL_W * cos(25), (i % 2) * PANEL_W * sin(25), 0])
    rotate([0, 0, (i % 2 == 0) ? 25 : -25]) color("black") panel();
}
