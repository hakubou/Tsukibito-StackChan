# 平安俳句ｽﾀｯｸﾁｬﾝ（仮称：文麿 / ふみまろ）

サークル制作の俳句ゲームのイベントで、来場者の一句を**短冊にして手渡す**付き人ロボット。
平安装束のｽﾀｯｸﾁｬﾝが文机に向かい、ミニ感熱プリンタで短冊を「したためて」献上する。

## ドキュメント
- [docs/concept.md](docs/concept.md) — 全体構想（体験の流れ・ハード/ソフト構成・開発ステップ）
- [docs/decoration.md](docs/decoration.md) — 装飾の詳細設計（衣装・顔・調度品・短冊デザイン・部品表）

## 機材
- M5Stack CoreS3 + Feetech SCS0009 ×2（パン/チルト）
- 感熱プリンタ: BluwieBroka S1（84×84×43mm、57mm紙、203dpi、BLE、アプリ「Luck Jingle」）
  - ⚠ ESP32 から BLE で直接制御できるかは未検証（プロトコル解析が必要）
- 3Dプリンタ

## フォルダ構成
- `src/` … ファームウェア（`~/Desktop/StackChan` のステップ1から派生）
- `cad/reference/takao/` … ｽﾀｯｸﾁｬﾝ takao 版 STL（参照用・ライセンスは同梱 LICENSE に従う）
- `cad/`（今後）… 烏帽子・文机カバーなどのオリジナル部品
