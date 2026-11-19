# 当日の朝の手順（理大祭 11/20）

付き人ｽﾀｯｸﾁｬﾝを、平安VR（市橋さんの [heian](https://github.com/tapiokapan184/heian)）の受付として動かす。
PC とは **USB** でつなぐ（Wi-Fi は使わない）。間に合わなければ、USB をつながず単体で動かすだけでもよい（呼び込み・声かけ・撫でる は単体で動く）。

```
Quest ──(歌)──> PC の印刷プログラム（print_server.py）──> レシートプリンタ
                      │ POST http://127.0.0.1:50090/event
                      ▼
                PC の中継（tools/stackchan_bridge.py）──USB──> ｽﾀｯｸﾁｬﾝ
```

## 1. Mac で書き込みと動作確認（ｽﾀｯｸﾁｬﾝを Mac につなぐ）

```bash
cd ~/Desktop/HeianStackChan && pio run -t upload && pio device monitor -b 115200
```

モニタで 1 文字ずつ送って確かめる（Enter は不要）。

| 確かめること | キー | 見るところ |
|---|---|---|
| 上を向ける限界 | `k` を 1 回ずつ（5° ずつ上）、`h` で戻す | 烏帽子・筐体が当たる手前の角度（`neck: tilt -25` など）→ `src/Neck.h` の `kTiltMin` |
| 顔追従 | 正面で顔を左右に動かす | 首が顔の方へついてくる（逃げるなら `kCameraMirrored` を反対に） |
| 結末の流れ | `1`（心が通った）/ `2`（たしなめられた）/ `3`（返歌なし） | セリフ → 「よい歌じゃった」→ 約9秒後にプリンタの方を向いて「紙を取ってくだされ」→ お辞儀 |
| プリンタの方向 | `1` | 向く方向がずれていたら `src/main.cpp` の `kPrinterPanDeg`（+で左）/ `kPrinterTiltDeg`（+で下） |
| 撫でる | 画面を指でなでる（または `7`） | 「くすぐったいわい」など |
| 音量 | `+` / `-` | 覚えておくので、書き込み直しても戻らない |
| 呼び込み・声かけ | `9` / `8` | 30〜60 秒おきに自動でも話す |

直したら、もう一度上のコマンドで書き込む。

## 2. ゲームの PC に中継を入れる

1. `tools/stackchan_bridge.py` と `tools/start_stackchan_bridge.bat` を、ゲームの PC の好きなフォルダにコピー（印刷プログラムと同じ `tools` でもよい）
2. ｽﾀｯｸﾁｬﾝを **ゲームの PC に USB でつなぐ**（Mac のモニタは閉じておく）
3. `start_stackchan_bridge.bat` をダブルクリック（初回は pyserial が入る）。「スタックチャンにつながりました（COMx）」が出れば OK
4. 別の窓で試し送り。ｽﾀｯｸﾁｬﾝが結末のセリフを話せば OK
   ```
   python stackchan_bridge.py --test kokoro
   ```
   ※ 中継の窓を開いたままだとポートが使用中で失敗する。そのときは中継を閉じてから試し、また開く。
5. 印刷プログラムの起動を、知らせ先をこの中継にする（`start_printer.bat` の `python print_server.py` の行の最後に足す）
   ```
   python print_server.py --stackchan http://127.0.0.1:50090/event
   ```
6. 通しの試し（中継の窓は開いたまま）。中継の窓に「知らせを送りました」、ｽﾀｯｸﾁｬﾝがセリフ
   ```
   python print_server.py --stackchan http://127.0.0.1:50090/event --stackchan-test kokoro
   ```

### うまくいかないとき
- 「スタックチャンが見つかりません」：USB を挿し直す。デバイスマネージャーの COM 番号を見て `start_stackchan_bridge.bat --port COM5`
- お辞儀のときに CoreS3 が再起動する：PC の USB だけでは電源が足りない。USB ハブ（電源付き）を通すか、背面の USB 口のほうに挿す
- 中継が止まっても、印刷は止まらない（印刷プログラムは知らせが届かなくても刷る）

## 3. 遊び始めの知らせ（市橋さん側でできれば）
`{"type":"game_start"}` を同じ住所に送ってもらえると、遊んでいる間は呼び込みを「いま歌を詠んでおられる」「次はそなたの番じゃ」に切り替え、
回数も減らす（VR の中の付き人の声と混ざらないように）。送られなければ、ずっと「誰も遊んでいない」ときの呼び込みのまま。
