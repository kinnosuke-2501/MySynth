# セッション引き継ぎドキュメント

## プロジェクト概要

**目標:** macOS スタンドアローン MIDI ソフトウェア音源  
**音色ターゲット:** ウーリッツァー/ローズ電子ピアノ。Arturia V Collection のような vintage 質感。ドリームポップ/シューゲイザー (Beach House, Artria 界隈) での使用を想定。  
**Tech Stack:** JUCE 8.x + C++17 + CMake 3.22+

---

## 現在の実装状態 (Phase 6-1 一部完了)

### ビルド・起動

```bash
# ビルド
cmake -B build -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build --config Debug -j$(sysctl -n hw.logicalcpu)

# 起動
open "build/MySynth_artefacts/Debug/MySynth.app"
```

### ファイル構成

```text
workspace_20260516/
├── CMakeLists.txt          # juce_dsp リンク済み、plist patch は cmake/patch_plist.cmake 経由
├── cmake/
│   └── patch_plist.cmake   # NSMidiUsageDescription を Info.plist に追記するスクリプト
├── JUCE/                   # git submodule (JUCE 8.x)
├── Source/
│   ├── Main.cpp            # JUCEエントリポイント (変更なし)
│   ├── MainComponent.h/cpp # AudioAppComponent + エフェクトチェーン管理
│   └── SynthVoice.h/cpp    # FM合成ボイス (Wurlitzer/Rhodes プリセット対応)
└── build/                  # ビルド成果物
```

### 音声合成アーキテクチャ

**シグナルフロー:**

```text
MIDIキーボード
  → MidiInput::openDevice (全デバイス自動オープン)
  → MidiMessageCollector (スレッドセーフキュー、CC#64も通過)
  → juce::Synthesiser (16ボイス ポリフォニー、サステインペダル自動処理)
  → ElectricPianoVoice × 16 (FM合成、プリセット参照)
  → Chorus (juce::dsp::Chorus, 25% wet)
  → Tremolo (LFO、ノブで可変)
  → Reverb (juce::dsp::Reverb、ノブで可変)
  → スピーカー出力
```

### FM合成エンジン (SynthVoice.cpp)

**Wurlitzer 200A デフォルトパラメータ:**

| パラメータ | 値 | 理由 |
| --- | --- | --- |
| Modulator比 | **2:1** | nasal/reedy な音色の核心 (Rhodesは1:1 = ベル系) |
| Modulator decay | 120ms (sustain=0%) | 鋭い "bark" → 素の音に素早く戻る |
| Velocity → FM depth | 0.8〜6.0 | Wurlitzer は非常にダイナミック |
| キースケーリング | ±30% | 低音域 bright、高音域 mellow (リード物理特性) |
| ノイズバースト | 20ms, velocity連動 | ハンマーがリードを叩く "clack" 再現 |
| L/R デチューン | ±3 cents | 自然なステレオ感 (Rhodesの±5より控えめ) |

**ADSRパラメータ (Wurlitzer デフォルト):**

```text
Carrier (音量エンベロープ): attack=1ms, decay=500ms, sustain=45%, release=500ms
Modulator (倍音エンベロープ): attack=0.5ms, decay=120ms, sustain=0%, release=60ms
```

**Phase 6-1 追加実装 (2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| 奇数倍音 | キャリアに 3次・5次倍音を追加。Wurlitzer は 3rd=10%, 5th=4%、Rhodes は 3rd=5% |
| 高音域補正 | C5 以上で modulator decay を短縮し、トランジェントを少し強める |
| ベロシティ音色カーブ | 音量は従来の `velPow` を維持しつつ、FM depth とノイズ量は logistic ベースの S字カーブで制御 |

---

## 現在のUI構成

```text
┌──────────────────────────────────────────────────────┐
│  Electric Piano          [WURLITZER] [RHODES]  ● ●   │
│  [status bar]                                         │
│  [hint label]                                         │
│                                                       │
│  TREMOLO RATE  TREMOLO DEPTH  REVERB WET  FM DEPTH    │
│  [knob]        [knob]         [knob]      [knob]      │
│  ATTACK        RELEASE                                │
│  [knob]        [knob]                                 │
│                                                       │
│ [Panic]                           [Settings...]       │
└──────────────────────────────────────────────────────┘
● = 発音中LED(緑) / サステインペダルLED(青)
```

**プリセットパラメータ差分:**

| パラメータ | Wurlitzer 200A | Rhodes Mark II |
| --- | --- | --- |
| FM比 (modRatio) | 2:1 (nasal/reedy) | 1:1 (bell/tine) |
| ステレオデチューン | ±3 cents | ±5 cents |
| モジュレータDecay | 120ms | 250ms |
| モジュレータSustain | 0% (完全消滅) | 20% (微妙に持続) |
| キャリアDecay | 500ms | 800ms |
| キャリアSustain | 45% | 70% |
| Velocity感度 | 0.8–6.0 (wide) | 0.5–3.0 (narrow) |
| ノイズバースト | 20% (reed clack) | 12% (tine tap) |
| Tremolo | 5Hz / 25% | 4Hz / 15% |
| Reverb Wet | 35% | 45% |

---

## 既知の課題・メモ

- **PlistBuddy の回避策:** `cmake/patch_plist.cmake` を介して `cmake -P` で実行している。これは shell エスケープ問題を回避するため。
- **juce_audio_processors は未リンク:** 現在は `juce_gui_app` ベースのスタンドアローンアプリ。プラグイン化する場合は要追加。
- **MIDIデバイスの自動検出:** `MidiInput::openDevice` で起動時に全デバイスを直接開く方式。ホットプラグ (起動後の接続) は非対応。
- **サステインペダル:** `juce::Synthesiser` が CC#64 を自動処理。`midiCollector` 経由で渡しているので追加実装不要。

---

## 全体ロードマップ

| Phase | 内容 | 状態 |
| --- | --- | --- |
| Phase 1 | 2-op FM合成 (Rhodes → Wurlitzer) + ステレオデチューン | ✅ 完了 |
| Phase 2 | Chorus + Tremolo + Reverb エフェクトチェーン | ✅ 完了 |
| Phase 3 | UIノブ追加 (Tremolo/Reverb/FM/ADSR) | ✅ 完了 |
| Phase 4 | Wurlitzer / Rhodes プリセット切り替え | ✅ 完了 |
| Phase 5 | サステインペダル対応 + 視覚インジケータ | ✅ 完了 |
| **Phase 6** | **音色ブラッシュアップ (倍音、ノンリニア特性)** | 進行中 (6-1 一部完了) |
