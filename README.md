# MySynth

macOS 向けのスタンドアローン電子ピアノ音源です。JUCE 8 と C++17 で実装しており、MIDI キーボードからの入力を受けて Wurlitzer / Rhodes 系の音色をリアルタイム合成します。

## 現在の実装内容

- JUCE ベースの macOS スタンドアローン GUI アプリ (800×520 固定サイズ)
- 16 ボイスのポリフォニー
- Wurlitzer / Rhodes プリセット切り替え
- 2-op FM をベースにした電子ピアノ音源
- 奇数倍音、buzz、ベロシティ音色カーブ、pitch bend、afterglow、sympathetic resonance を含む音色ブラッシュアップ
- ボイス生成の 2× オーバーサンプリングによるアンチエイリアス、位相ラップ、DC ブロッカー
- EP らしい 2段指数ディケイ振幅エンベロープ (フラット sustain を廃し自然に減衰)
- MASTER ボリューム + 出力ソフトリミッタ (クリップ保護 / DRIVE 併用でアンプ的歪み)
- プリディレイ + wet 高域ダンプ付きセンド型リバーブ、Rhodes はステレオ・オートパン (実機 Suitcase 準拠)
- 状態永続化 (終了時に保存し、起動時にプリセット・全ノブ・MASTER を復元)
- ノブ操作: ダブルクリックで既定値復帰 / マウスホイール / Cmd・Ctrl ドラッグで微調整
- プリセット管理: ヘッダ中央の選択ボックスでユーザープリセットを保存/読込/削除 (ファクトリ2種＋ユーザー)
- Chorus / Tremolo / Reverb のエフェクトチェーン
- サステインペダル (CC#64) 対応 + ペダルノイズ
- VintageLookAndFeel (真鍮ノブ・ビンテージ配色) + VU メーター + LED インジケータ
- 起動時の全 MIDI 入力デバイス自動オープン
- Audio / MIDI 設定ダイアログと Panic ボタン

## 必要環境

- macOS
- CMake 3.22 以上
- Xcode Command Line Tools
- JUCE サブモジュール

## セットアップ

```bash
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build --config Debug --target MySynth -j$(sysctl -n hw.logicalcpu)
```

## 起動

```bash
open "build/MySynth_artefacts/Debug/StageFM.app"
```

または補助スクリプトを使えます。

```bash
./scripts/run_debug.sh
```

## 配布（友達に渡す / Web で配る）

未署名のまま無料で配布できます（Apple Developer Program 不要）。

```bash
./scripts/package_release.sh   # Release Universal をビルド → dist/StageFM-<ver>-macOS.zip
./scripts/publish_release.sh   # ↑を GitHub Releases に公開 + 配布ページ(GitHub Pages)を更新
```

- `package_release.sh`: Apple Silicon / Intel 両対応（Universal, macOS 11+）の `.app` を zip 化。
- `publish_release.sh`: **外向き公開**。GitHub Release 作成と `site/` を `gh-pages` へデプロイ、Pages 有効化。
- 配布ページ: `https://kinnosuke-2501.github.io/MySynth/`（インストール手順入り。`site/index.html`）。
- 未署名のため初回は **右クリック →「開く」** が必要（ページに手順記載）。警告を消すには Apple Developer Program 署名・公証が必要（将来）。
- アプリアイコン: `Assets/icon_1024.png`（`Assets/make_icon.py` で再生成可）。

## ディレクトリ構成

```text
.
├── README.md
├── docs/
│   ├── README.md
│   ├── PRODUCT_ROADMAP.md
│   └── SESSION_CONTEXT.md
├── Source/
├── Assets/          # アプリアイコン (icon_1024.png + 生成スクリプト)
├── site/            # GitHub Pages 配布ページ
├── cmake/
├── scripts/         # run_debug / package_release / publish_release
├── JUCE/
└── CMakeLists.txt
```

## ドキュメント

- [docs/README.md](docs/README.md): ドキュメント一覧
- [docs/PRODUCT_ROADMAP.md](docs/PRODUCT_ROADMAP.md): 品質改善ロードマップ
- [docs/SESSION_CONTEXT.md](docs/SESSION_CONTEXT.md): 現在の実装状態と引き継ぎメモ

## メモ

- macOS の MIDI 利用説明文は [cmake/patch_plist.cmake](cmake/patch_plist.cmake) で `Info.plist` に追記しています。
- 現状は `juce_add_gui_app()` ベースのスタンドアローンアプリです。
