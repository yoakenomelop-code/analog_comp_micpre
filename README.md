# Vintage Rack（AU / VST3）

クラスA のマイクプリ＆インダクター EQ（**Pre / EQ**）、FET コンプレッサー（**FET Comp**）、光学式レベラー（**Opto Leveler**）を、
**1つのラック画面で好きな数だけ並べて操作できる**プラグインです。Web 版と同じ DSP（音のアルゴリズム）を C++ に移植しています。

各モジュールは往年のスタジオ機材の音響特性を参考に独自に設計したもので、それらの機材のメーカーとは一切関係がありません（[参考にした機材と商標について](#参考にした機材と商標について)）。

## 入っているプラグイン

| プラグイン名 | 中身 | AU コード |
|---|---|---|
| **Vintage Rack** | 最大 8 台まで自由に追加・複製・削除・並べ替え（同じモジュールを複数台も可） | `aufx VRak VtRk` |
| VR Pre EQ | Pre / EQ 単体（Classic / Classic+ 切替） | `aufx VR73 VtRk` |
| VR FET Comp | FET Comp 単体（Early / Low-noise / Late 切替） | `aufx VR76 VtRk` |
| VR Opto Leveler | Opto Leveler 単体 | `aufx VR3a VtRk` |

各プラグインとも AU（Mac）、VST3（Mac / Windows）、Standalone が作られます。

## 画面の見かた

- **FREQUENCY MAP**：EQ の各バンドを色分け表示。つまみにカーソルを合わせると、そのつまみが動かす帯域が同じ色で光ります。グラフ上をなぞると「その周波数を動かすのはどのつまみか」が表示されます。
- **ANALOG**（各モジュールのヘッダー）：オンで倍音（サチュレーション）とトランス・回路による帯域のクセを加えます。オフで完全にクリーン。ボタンにカーソルを合わせると、そのモジュールのアナログ特性カーブがグラフ上で光ります。変化は ±1 dB 前後なので、ツールバーの「縦軸 ±2 dB」で拡大すると見やすくなります。上部の「ANALOG 全オン／全オフ」で一括切替。
- **無段階**（Pre / EQ と Opto）：固定ポイントの代わりに、周波数つまみを連続可変にします（縁が点線のつまみ）。範囲は HPF 10–600 Hz（左端で OFF）、LOW 20–500 Hz、MID 150 Hz–10 kHz、HIGH 3–20 kHz、LPF 2–22 kHz（右端で OFF、Classic+ のみ）、Opto の検出帯域 0.5–10 kHz。
- **回路（CIRCUIT）**（各モジュールの「回路」ボタン）：内部回路の部品と挙動を編集できます。
  - Pre / EQ：入力トランス（Vintage〔ニッケルコア〕／Mid-era／Modern reissue／トランスレス）、増幅回路（ヴィンテージ NOS／現行／現代ローノイズ）、出力トランス（Vintage／Modern reissue／トランスレス）
  - FET Comp：FET（初期型／後期型／選別なし NOS／選別ローノイズ）、入力トランス（ニッケル／スチール／トランスレス）、出力段（Class-A＋トランス／トランスレス／ホットロッド）。「モデル標準」は選んだモデルの部品を使います
  - Opto Leveler：光学セル（新品／エージング／高速改造）、出力アンプ（ディスクリート／IC）、出力トランス
  - つまみ：BIAS（偶数倍音）、HEADROOM（電源電圧 18–30 V）、IRON（トランス飽和 0–200 %）、AGE（経年劣化・個体差：高域のくすみ、歪み・ノイズ増、左右の EQ 周波数ずれ）、NOISE（ヒス＋50 Hz ハム）、FET Comp は DETECTOR、Opto は MEMORY（リリースのプログラム依存度）
  - 右側のグラフに、100 Hz / 1 kHz（入力 −12 dBFS）の倍音 H2〜H7 と THD を表示します。部品による帯域のクセは周波数マップの点線（ANALOG）に反映されます
- **プリセット**：ヘッダーの「ラックプリセット…」でチェーン全体（22 種：ボーカル〔ロック／なめらか／直列コンプ／空気感〕、エレキ〔クリーンのピック単音・指弾きアルペジオ・ストラム、オーバードライブとディストーションのピック単音・ストラム〕、ドラム〔キック／スネア／オーバーヘッド／自然なルーム／ルーム・スマッシュ／ドラムバス〕、ベースDI、アコギ、ミックスバス、ローファイ）、各モジュールの「プリセット…」で単体の設定（Pre / EQ：20 種、FET：12 種、Opto：12 種）を読み込めます。「初期値」以外は入力とほぼ同じ音量になるよう調整済みです。DAW のプログラム（プリセット）一覧からも選べます。プリセットは `Resources/presets.json`（Web 版と共通）にあります
- **ラック操作**（Vintage Rack のみ）：ツールバーの「＋」で追加、各モジュールの「複製」「削除」、左端の点をドラッグ／▲▼ で並べ替え。上から順に信号が流れます。

## ビルド方法

### いちばん簡単：GitHub に置いて自動ビルド（Mac / Windows 両方）
1. このフォルダを GitHub のリポジトリにアップロード（push）します。
2. リポジトリの **Actions** タブで `build` が自動で走ります（20〜30 分程度）。
3. 終わったら `VintageRack-mac`（AU + VST3）と `VintageRack-windows`（VST3）をダウンロード。
   Mac 版は CI 上で pluginval による検証も実行されます（結果はログで確認できます）。

### Mac で自分でビルド
必要なもの：Xcode（App Store）、CMake（`brew install cmake`）
```bash
cmake -B build -G Xcode -DVR_COPY_AFTER_BUILD=ON
cmake --build build --config Release
```
`VR_COPY_AFTER_BUILD=ON` で `~/Library/Audio/Plug-Ins/Components`（AU）と `~/Library/Audio/Plug-Ins/VST3` に自動でコピーされます。
初回は JUCE 8.0.4 を自動ダウンロードします（ネット接続が必要）。

AU が Logic で見えない場合：
```bash
killall -9 AudioComponentRegistrar
auval -v aufx VRak VtRk     # Vintage Rack の検証（他は上の表のコード）
```

### GitHub Actions でビルドしたものを Mac で使う
1. ダウンロードした `VintageRack-mac` の中の `VintageRack-mac.zip` をダブルクリックで展開し、
   `Components` の中身と `VST3` の中身を、プラグインフォルダにコピーします。
   - 自分のユーザーだけ：`~/Library/Audio/Plug-Ins/Components` と `~/Library/Audio/Plug-Ins/VST3`
   - Mac 全体：`/Library/Audio/Plug-Ins/Components` と `/Library/Audio/Plug-Ins/VST3`
2. 正式な署名がないため「Mac を保護するためにブロックされました」と出ます。ターミナルで次を実行してください。
   両方のフォルダを探し、見つかったプラグインだけ隔離属性を外して実行権限を付けます。処理したものは `OK:` と表示されます。
   最初に Mac のログインパスワードを聞かれます（入力しても画面には表示されません）。
```bash
for d in "$HOME/Library/Audio/Plug-Ins" "/Library/Audio/Plug-Ins"; do
  for p in "VST3/Vintage Rack.vst3" "VST3/VR Pre EQ.vst3" "VST3/VR FET Comp.vst3" "VST3/VR Opto Leveler.vst3" \
           "Components/Vintage Rack.component" "Components/VR Pre EQ.component" "Components/VR FET Comp.component" "Components/VR Opto Leveler.component"; do
    [ -e "$d/$p" ] || continue
    sudo xattr -dr com.apple.quarantine "$d/$p"
    sudo chmod +x "$d/$p"/Contents/MacOS/*
    echo "OK: $d/$p"
  done
done
killall -9 AudioComponentRegistrar 2>/dev/null
```
3. DAW を起動し直してプラグインを再スキャンします。Ableton Live では、設定 → プラグイン で「VST3 プラグインシステムフォルダを使用」「Audio Units を使用」をオンにし、**Option キーを押しながら「再スキャン」**（全体の再スキャン）。

### Windows で自分でビルド
必要なもの：Visual Studio 2022（「C++ によるデスクトップ開発」）、CMake
```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
できた `build\<名前>_artefacts\Release\VST3\*.vst3` を `C:\Program Files\Common Files\VST3` にコピーします。

## 音作りの中身（Web 版と共通）

- **Pre / EQ**：段階ゲイン（MIC 20–80 dB / LINE −10–+20 dB）、HPF 18 dB/oct（50/80/160/300 Hz）、LOW シェルフ（35/60/110/220 Hz）、MID ベル（0.36–7.2 kHz の 6 点）、HIGH シェルフ（Classic は 12 kHz 固定、Classic+ は 10/12/16 kHz）、Classic+ は HI-Q と LPF（18/10/8/6 kHz）。
- **FET Comp**：ATTACK 20–800 µs、RELEASE 50 ms–1.1 s（どちらも 7 が最速）、RATIO 4/8/12/20/ALL。モデル（Early / Low-noise / Late）ごとに倍音量・ニー・帯域のクセが違います。
- **Opto Leveler**：アタック 1.5 ms 以下（大きな入力ほど速い）、リリースは 60 ms で 50 %、残りは 0.5〜5 秒（長く圧縮するほど遅くなる）。COMP / LIMIT、HF EMPH（検出回路だけの高域強調）。
- 全体を 2 倍オーバーサンプリングで処理（レイテンシ 4 サンプル程度、ホストに自動報告）。

アナログ特性（帯域のクセ・倍音）は傾向をモデル化したもので、特定の機材の測定データの再現ではありません。ノイズとハムは CIRCUIT の NOISE で加えられます（初期値はオフ）。
控えめな音にしたい場合は、Pre / EQ の入力トランスを「Modern reissue」、増幅回路を「Current transistors」にしてください。

## 確認済みのこと
- Linux 上で 4 プラグインすべてのビルドが通ること、GitHub Actions で Mac（AU + VST3）と Windows（VST3）のビルドが通ること
- Mac の VST3 4 種が pluginval（strictness 5）に合格すること
- C++ 版と Web 版の DSP 出力が一致すること（同じ入力でビット単位で一致）
- EQ の実測レスポンスがグラフ表示と 0.1 dB 以内で一致すること
- Opto の 50 % リリースが約 60 ms、FET Comp の ANALOG オフで倍音が消えること
- スロットの追加・複製・移動・削除、状態の保存／復元、NaN が出ないこと

## 注意
- Vintage Rack でモジュールを並べ替えると、スロット番号に紐づくオートメーションは「位置」についていきます（S1 のオートメーションは常に 1 番目のモジュール）。
- 単体プラグインの名前を変更しました（旧：VR 73 Pre EQ / VR 76 FET / VR 3A Opto）。内部の識別コードは同じなので、保存済みのセッションはそのまま開けます。古い名前のファイルがプラグインフォルダに残っている場合は削除してください。

## ライセンス

このリポジトリのソースコードは [GNU Affero General Public License v3.0](LICENSE)（AGPLv3）で公開しています。
- 使用しているフレームワーク [JUCE](https://juce.com) は AGPLv3 と JUCE ライセンスのデュアルライセンスです。本リポジトリは AGPLv3 の条件で JUCE を利用しています。ソースを公開せずに配布・販売する場合は、JUCE ライセンス（収益条件を満たせば無料の Starter など）が別途必要です。
- VST は Steinberg Media Technologies GmbH の商標です。Audio Units は Apple Inc. の商標です。対応形式を示す目的でのみ記載しています。

## 参考にした機材と商標について

本ソフトウェアの各モジュールは、次の機材の一般に知られている音響特性・仕様を参考に、独自に設計・実装したものです。
回路や部品の選択肢は、それらの傾向を表す説明的な名前で表記しています。

| モジュール | 参考にした機材 |
|---|---|
| Pre / EQ（Classic / Classic+） | Neve 1073 / 1084 |
| FET Comp（Early / Low-noise / Late） | UREI / Universal Audio 1176 の各リビジョン |
| Opto Leveler | UREI / Universal Audio LA-3A |

本ソフトウェアおよびその作者は、AMS Neve Limited、Universal Audio, Inc.、その他の機材・部品メーカーとは一切関係がなく、承認・提携・後援を受けていません。
Neve、1073、Marinair は AMS Neve Limited の、1176、1176LN、LA-2A、LA-3A、Teletronix は Universal Audio, Inc. の商標または登録商標です。
その他の製品名・会社名は各社の商標または登録商標です。これらの名称は、参考にした機材を特定・説明する目的でのみ使用しています。
