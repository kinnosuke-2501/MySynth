# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

macOS向けスタンドアローンMIDIソフトウェア音源。USB/DINのMIDIキーボードからNote On/Offを受け取り、リアルタイムで音声合成して出力する。

**対象ユーザー:** 業務アプリ・AWSインフラ経験者だがオーディオプログラミングは未経験。

## Tech Stack

| Layer | Technology |
|---|---|
| オーディオ/MIDIフレームワーク | [JUCE](https://juce.com/) 8.x |
| 言語 | C++17 |
| ビルドシステム | CMake 3.22+ |
| UI | JUCE Component (JUCEネイティブ) |
| ターゲット | macOS Standalone Application |

JUCEはプロのDAWプラグイン・スタンドアローン音源の業界標準。`AudioProcessor` + `StandalonePluginHolder` でスタンドアローンとして動く。

## Build Commands

```bash
# 依存関係インストール (初回のみ)
brew install cmake

# JUCEをsubmoduleとして追加 (初回のみ)
git submodule add https://github.com/juce-framework/JUCE.git JUCE

# ビルド設定
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# ビルド実行
cmake --build build --config Debug -j$(sysctl -n hw.logicalcpu)

# アプリを起動
open "build/<ProjectName>_artefacts/Debug/Standalone/<ProjectName>.app"
```

Releaseビルド:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(sysctl -n hw.logicalcpu)
```

## Architecture

### ディレクトリ構成（設計）

```
workspace_20260516/
├── CMakeLists.txt          # JUCEのjuce_add_gui_app or juce_add_plugin
├── JUCE/                   # git submodule
└── Source/
    ├── Main.cpp            # JUCEエントリポイント (START_JUCE_APPLICATION)
    ├── MainComponent.h/cpp # メインUI + AudioDeviceManager管理
    ├── SynthEngine.h/cpp   # 音声合成エンジン (AudioSource or AudioProcessor)
    ├── SynthVoice.h/cpp    # 1音ごとの合成ロジック (オシレータ+ADSR)
    └── MidiHandler.h/cpp   # MidiInputCallback実装
```

### JUCE オーディオアーキテクチャ

```
[MIDIキーボード]
      |
      v
[MidiInput] --- MidiInputCallback::handleIncomingMidiMessage()
      |
      v (MidiBuffer経由)
[AudioDeviceManager]
      |
      v
[AudioIODeviceCallback::audioDeviceIOCallbackWithContext()]
      |  ← ここで SynthEngine.processNextBlock() を呼ぶ
      v
[スピーカー出力]
```

- **オーディオコールバック** はリアルタイムスレッドで動く。メモリアロケーション・mutex・重い処理は禁止。
- **MIDIイベントとオーディオ** はバッファ単位 (通常128〜512サンプル) で同期する。
- `juce::Synthesiser` クラスを使うと `SynthVoice` のポリフォニー管理が簡単になる。

### 音声合成の基本単位

```
SynthVoice (juce::SynthesiserVoice を継承):
  - Oscillator (波形生成: サイン波, ノコギリ波, 矩形波)
  - ADSR Envelope (Attack/Decay/Sustain/Release)
  - startNote() / stopNote() / renderNextBlock()
```

### CMakeLists.txt の基本形

```cmake
cmake_minimum_required(VERSION 3.22)
project(MySynth VERSION 0.1.0)

add_subdirectory(JUCE)

juce_add_gui_app(MySynth
    PRODUCT_NAME "MySynth"
    COMPANY_NAME "MyName"
)

target_sources(MySynth PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/SynthEngine.cpp
    Source/SynthVoice.cpp
)

target_compile_features(MySynth PRIVATE cxx_std_17)

target_link_libraries(MySynth PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
)
```

## MIDI / オーディオ の重要概念

- **サンプルレート**: 通常44100 or 48000 Hz。1秒間に44100サンプルの数値列で音を表現。
- **バッファサイズ**: 1コールバックで処理するサンプル数 (128〜512)。小さいほどレイテンシ低いがCPU高負荷。
- **MIDIノート番号**: 0〜127。中央のC(C4) = 60。周波数 = 440 * 2^((note-69)/12)。
- **ベロシティ**: 0〜127。キーを押す強さ。音量・音色変化に使う。
- **ポリフォニー**: 同時発音数。`juce::Synthesiser` に最大ボイス数を渡して管理。

## macOS 固有の設定

- **Entitlements**: マイク不要だがオーディオデバイスアクセスには `com.apple.security.device.audio-input` が必要な場合がある（スタンドアローンはJUCEが処理）。
- **MIDI アクセス**: macOS 15+ では `NSMidiUsageDescription` が Info.plist に必要。JUCE 8 は自動で処理するが、権限エラーが出たら確認すること。
- **Apple Silicon**: `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` でUniversal Binaryをビルドできる。

## 開発の進め方 (推奨順序)

1. CMakeLists.txt + JUCEサブモジュールでビルドが通る状態を作る
2. `MainComponent` でサイン波を鳴らすだけのオーディオ出力を実装
3. MIDIキーボードのNote Onを受け取ってコンソール出力で確認
4. MIDIノートに対応した周波数のサイン波を鳴らす (単音)
5. `juce::Synthesiser` + `SynthVoice` でポリフォニー対応
6. ADSRエンベロープを追加
7. 波形選択 (サイン/ノコギリ/矩形) のUI追加
