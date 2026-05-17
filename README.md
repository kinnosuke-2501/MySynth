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
open "build/MySynth_artefacts/Debug/MySynth.app"
```

または補助スクリプトを使えます。

```bash
./scripts/run_debug.sh
```

## ディレクトリ構成

```text
.
├── README.md
├── docs/
│   ├── README.md
│   ├── PRODUCT_ROADMAP.md
│   └── SESSION_CONTEXT.md
├── Source/
├── cmake/
├── scripts/
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
