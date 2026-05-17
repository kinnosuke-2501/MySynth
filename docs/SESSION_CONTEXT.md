# セッション引き継ぎドキュメント

## プロジェクト概要

**目標:** macOS スタンドアローン MIDI ソフトウェア音源  
**音色ターゲット:** ウーリッツァー/ローズ電子ピアノ。Arturia V Collection のような vintage 質感。ドリームポップ/シューゲイザー (Beach House, Artria 界隈) での使用を想定。  
**Tech Stack:** JUCE 8.x + C++17 + CMake 3.22+

---

## 現在の実装状態 (Phase 8: 正しさ + アンチエイリアス対応 — 2026-05-17)

> 最新サマリ:
>
> - Phase 6 (音色ブラッシュアップ) / Phase 7 (UI/UX 全面リデザイン) 完了。
> - レビューに基づき「正しさ三点パック」(位相ラップ・DCブロッカー・ウィンドウ固定サイズ化) を実施。
> - 音質の最大改善項目として **ボイス生成の 2× オーバーサンプリング** を実装 (anti-aliasing)。
> - 次の着手候補: 2段指数ディケイ (#5)、マスターVol+リミッタ (#6)、リバーブ刷新 (#7)。

### ビルド・起動

```bash
# ビルド
cmake -B build -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build --config Debug -j$(sysctl -n hw.logicalcpu)

# 起動
./scripts/run_debug.sh
```

### ファイル構成

```text
workspace_20260516/
├── CMakeLists.txt          # juce_dsp リンク済み、plist patch は cmake/patch_plist.cmake 経由
├── docs/
│   ├── PRODUCT_ROADMAP.md  # 品質改善ロードマップ
│   └── SESSION_CONTEXT.md  # この引き継ぎドキュメント
├── cmake/
│   └── patch_plist.cmake   # NSMidiUsageDescription を Info.plist に追記するスクリプト
├── docs/
│   ├── PRODUCT_ROADMAP.md
│   └── SESSION_CONTEXT.md
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

**Phase 6-2 追加実装 (2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| Wurlitzer buzz | `satDrive` をプリセット側の reed clipping drive として使用し、アタック時だけ強く出る tanh ベースの buzz 段を追加 |
| ダイナミクス | buzz は `modulatorEnv` と note-on velocity から計算した `buzzMix` で制御。強打時に bark が増し、サステインでは薄くなる |

**Phase 6-3 追加実装 (2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| Pitch bend | `ElectricPianoVoice::pitchWheelMoved()` を実装し、現在ノートの基準周波数から oscillator delta を再計算 |
| レンジ | デフォルトは `pitchBendRange = 2.0f` で ±2 半音。UI 追加前の内部パラメータとして保持 |
| note-on同期 | `startNote(..., currentPitchWheelPosition)` から現在の wheel 値を適用し、bend した状態で押したノートも正しく追従 |

**Phase 6-4 追加実装 (2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| Rhodes afterglow | `stopNote()` で note-off 時に bell tail を起動し、`renderNextBlock()` で 4次倍音ベースの高域 tail を減衰させる |
| プリセット差分 | Wurlitzer は `afterglowAmount=0`、Rhodes は note-off 後の shimmer が乗るよう `afterglowAmount=0.22`, `afterglowDecay=0.24s` |
| Sympathetic resonance | サステインペダル中のみ、Rhodes に薄い tonebar/body 共鳴を追加。キーを離した後の sustain tail で少し目立つように制御 |
| Pedal noise | `MainComponent` で CC#64 のエッジを検出し、pedal down は低めの thunk、pedal up は短い click をグローバルに加算 |
| 未実装 | pedal noise は初回実装のみ。質感の追い込みは今後の調整対象 |

**Phase 7 完了 (UI/UX 全面リデザイン — 2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| VintageLookAndFeel | `Source/VintageLookAndFeel.h/cpp` 新設。真鍮ロータリーノブ・ハードウェア風ボタン・ビンテージ配色を一元管理 |
| レイアウト | 800×520 に拡張。SOUND / FX セクション分割、ヘッダ・キーボードベイ・フッタ構成 |
| VU メーター | オーディオスレッドで RMS を atomic 書き込み、Timer (~12fps) で dB 目盛付きグラデーションバー描画 |
| ノブ追加 | CHORUS ノブを追加 (SOUND 4 + FX 4 の計 8 ノブ構成) |
| LED | 発音中 (アンバー) / サステイン (シアン) のハロー付き LED |

**Phase 8 完了 (正しさ三点パック + アンチエイリアス — 2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| 位相ラップ | `SynthVoice.cpp` で carrier/modulator 角度を毎サンプル `[0,2π)` に巻き戻し。ロングトーンでの float 精度劣化 (デチューン/FMノイズ) を除去 |
| DC ブロッカー | `MainComponent` に 1次 HPF (~20Hz, サンプルレート連動)。非対称リードシェイピングの DC オフセットを synth レンダ直後・FX 前に除去 |
| ウィンドウ固定 | `Main.cpp` を `setResizable(false,false)` に変更。「リサイズ可能なのに固定レイアウト」の破綻バグを最小リスクで解消 (商用音源と同じ固定サイズ方式) |
| **2× オーバーサンプリング** | **ボイス生成のみ** を `juce::dsp::Oversampling` (2×, halfband polyphase IIR, 低レイテンシ) で生成→デシメート。FM + 倍音加算 + 非対称tanh + buzz + サチュレーションのエイリアシングを抑制。FX は base レート据え置きで CPU 増を生成段に限定 |

> オーバーサンプリング実装の要点:
>
> - `prepareToPlay` で `synthesiser.setCurrentPlaybackSampleRate(2×fs)` / `midiCollector.reset(2×fs)`。ボイスは全て `getSampleRate()` 基準なのでピッチ・ADSR・ノイズ減衰が自動追従。
> - `getNextAudioBlock` は base ブロックを `processSamplesUp` → 内部 2× バッファに synth を additive レンダ → `processSamplesDown` でアンチエイリアス・デシメート。MIDI は `osNumSamples = numSamples × 2` で取得。
> - 既知の前提: コードベース全体がステレオ前提 (`setAudioChannels(0,2)`)。モノデバイス時のフォールバックは未対応 (Phase 8 で導入したものではなく既存の前提)。
> - レイテンシ: IIR polyphase の付加レイテンシはごく小さく未補償 (実用上問題なし、将来 `getLatencyInSamples()` 補償を検討)。

---

## 現在のUI構成

800×520 固定サイズ。VintageLookAndFeel 適用済み。

```text
┌─────────────────────────────────────────────────────────────┐
│  ELECTRIC PIANO                  [WURLI] [ROADS]  (SUS)(KEY) │ ヘッダ
│  STAGE FM ELECTRIC PIANO                                     │
├──────────────────────────┬──────────────────────────────────┤
│  SOUND                   │  FX                               │
│  FM DEPTH ATTACK         │  TREM RATE TREM DEPTH             │
│  RELEASE  DRIVE          │  REVERB    CHORUS                 │
│  [knob]×4                │  [knob]×4                         │
├─────────────────────────────────────────────────────────────┤
│  [ MIDI KEYBOARD (モデル別レンジ) ]                          │
├─────────────────────────────────────────────────────────────┤
│  [━━━━━━━━━━ VU メーター (dB目盛) ━━━━━━━━━━]                │ フッタ
│  [Panic]   status / MODEL 名プレート     [Settings...]       │
└─────────────────────────────────────────────────────────────┘
LED: KEY=発音中(アンバー) / SUS=サステイン(シアン)
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
- **ウィンドウサイズ:** 800×520 固定 (`setResizable(false,false)`)。比率レイアウト化は将来タスク。これに伴い `paint`/`resized` の幾何二重計算は実害のないコスメティック負債に格下げ。
- **ステレオ前提:** `setAudioChannels(0,2)`。オーバーサンプラーも 2ch 構成。モノデバイス時のフォールバック未対応。
- **オーバーサンプリング:** ボイス生成のみ 2×。FX は base レート。倍率は `MainComponent.h::kOversampleFactorLog2` で変更可 (1→2×, 2→4×)。

---

## 全体ロードマップ

| Phase | 内容 | 状態 |
| --- | --- | --- |
| Phase 1 | 2-op FM合成 (Rhodes → Wurlitzer) + ステレオデチューン | ✅ 完了 |
| Phase 2 | Chorus + Tremolo + Reverb エフェクトチェーン | ✅ 完了 |
| Phase 3 | UIノブ追加 (Tremolo/Reverb/FM/ADSR) | ✅ 完了 |
| Phase 4 | Wurlitzer / Rhodes プリセット切り替え | ✅ 完了 |
| Phase 5 | サステインペダル対応 + 視覚インジケータ | ✅ 完了 |
| Phase 6 | 音色ブラッシュアップ (倍音、ノンリニア特性、afterglow、resonance、pedal noise) | ✅ 完了 |
| Phase 7 | UI/UX 全面リデザイン (VintageLookAndFeel、800×520、VU メーター) | ✅ 完了 |
| Phase 8 | 正しさ三点パック (位相ラップ・DCブロッカー・固定サイズ) + 2× オーバーサンプリング | ✅ 完了 |
| **Phase 9** | **2段指数ディケイ / マスターVol+リミッタ / リバーブ刷新 / 状態永続化** | 未着手 (次の優先) |
