# セッション引き継ぎドキュメント

## プロジェクト概要

**目標:** macOS スタンドアローン MIDI ソフトウェア音源  
**音色ターゲット:** ウーリッツァー/ローズ電子ピアノ。Arturia V Collection のような vintage 質感。ドリームポップ/シューゲイザー (Beach House, Artria 界隈) での使用を想定。  
**Tech Stack:** JUCE 8.x + C++17 + CMake 3.22+

---

## 現在の実装状態 (Phase 9 進行中 — 2026-05-17)

> 最新サマリ:
>
> - Phase 6 (音色) / Phase 7 (UI/UX) / Phase 8 (正しさ三点パック + 2× OS) 完了。
> - Phase 9-1: キャリア振幅エンベロープを **2段指数ディケイ (EpAmpEnvelope)** に置換。
>   juce::ADSR のフラット sustain を廃し、EP らしい「鳴って減衰し続ける」挙動へ。
> - Phase 9-2: **マスターボリューム + 出力ソフトリミッタ** を実装。MASTER ノブを
>   FX セクションに追加 (グローバル設定のためプリセットでリセットしない)。
> - Phase 9-3: フィードバック対応。Wurli アタックを実機寄りに緩和、MASTER レンジ拡張
>   (DRIVE 併用でアンプ的歪み)。RELEASE 一定は実機仕様につき意図的 (変更なし)。
> - Phase 9-4: **リバーブ刷新 (プリディレイ+wet高域ダンプ+センド構成) + Rhodes
>   パントレモロ** (Suitcase の vibrato は実機ではステレオオートパン)。
> - Phase 9-5: **状態永続化** (ApplicationProperties)。終了時に保存、起動時に
>   プリセット+全ノブ+MASTER を復元。ラウンドトリップ実地検証済み。
> - Phase 9-6: **描画最適化**。12fps 全面 repaint を廃し、30fps で VU/LED の
>   dirty rect のみ・変化時のみ無効化。見た目は不変。
> - Phase 9-7: **ノブ UX 標準化**。全ノブにダブルクリックで既定値復帰
>   (プリセット系は現プリセット値へ)、ホイール、Cmd/Ctrl 微調整。
> - **設計原則確定: 音作りは要望より実機挙動の再現を優先**。
> - 次候補: プリセット管理 (#10 後半・UI 設計判断要) / 配布 (#11)。

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
  → juce::Synthesiser (16ボイス、2×OS で生成→デシメート)
  → ElectricPianoVoice × 16 (FM合成、EpAmpEnvelope、プリセット参照)
  → DCブロッカー (~20Hz HPF)
  → Chorus (juce::dsp::Chorus)
  → Tremolo (Wurli=振幅 / Rhodes=ステレオオートパン)
  → ペダルノイズ加算
  → Reverb センド (プリディレイ25ms → 100%wet reverb → wet 7kHz LPF → dry に加算)
  → MASTER ゲイン → 出力ソフトリミッタ
  → VU 計測 → スピーカー出力
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

**Phase 9-1 完了 (2段指数ディケイ — 2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| EpAmpEnvelope | `SynthVoice.h` に新規。Attack(線形) → Decay1(膝レベルへ指数接近) → Decay2(膝→無音の遅い指数ボディ減衰) → Release(指数) のステートマシン。juce::ADSR と同一 API でキャリアのみ差し替え |
| 設計意図 | juce::ADSR のフラット sustain を廃止。本物の Rhodes/Wurlitzer は鍵盤を押し続けても sustain せず減衰し続ける ⇒ オルガン感の解消 |
| パラメータ再解釈 | 既存 `carrierDecay`→Decay1 時間、`carrierSustain`→膝レベル(Decay1 が落ち着く点)、新規 `carrierDecay2`→ボディ減衰時間 |
| プリセット値 | Wurlitzer: Decay1=0.35s, 膝=0.40, Decay2=3.5s (リードは減衰早め)。Rhodes: Decay1=1.5s, 膝=0.80, Decay2=8.0s (tine が長く歌う) |
| 係数 | `pow(0.001, 1/(sr·T))` (−60dB to T)。ファイル内の afterglow/noise 減衰と同一イディオム。サンプルレートは 2×fs (OS) に自動追従 |
| 範囲限定 | モジュレータ (brightness) は短く可聴影響小のため juce::ADSR 維持。変更は SynthVoice.h/cpp + PresetData/applyPreset のみ |

> 挙動の変更点: 鍵盤を押し続けた音は (旧: フラット保持) → (新: ゆっくり減衰し最終的に無音化しボイス解放)。
> これは実機 EP の正しい挙動。Decay2 を十分長く取り和音保持が早く消えないよう調整済み。

**Phase 9-2 完了 (マスターボリューム + 出力ソフトリミッタ — 2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| マスターボリューム | `masterGain` (atomic, 既定 0.8)。MASTER ノブ (0–100%) を FX セクションに追加。**グローバル設定のため applyPreset で snap しない** (プリセット切替で音量が変わらない正しい挙動) |
| 出力ソフトリミッタ | 最終段 (リバーブ後・VU前)。絶対値 0.8 以下は線形、超過分のみ tanh で ±1.0 に漸近。多ボイス+リバーブ tail の合算が 0dBFS を超えてもハードクリップが DAC に到達しない |
| UI レイアウト | FX セクションを `/4 → /5` に拡張 (1セクション局所変更)。SOUND グリッド (/4) は不変 |
| 判断根拠 | リミッタは安全機能ゆえ無条件実装。MASTER は専用セクション化が理想だが低リスク優先で FX 5分割。専用マスターUIは将来のUX磨き (ロードマップ将来枠) |
| VU 計測位置 | リミッタ後に移動済みのため VU は実際の可聴出力を反映 |

**Phase 9-3 完了 (フィードバック対応・実機準拠の音作り — 2026-05-17):**

> 設計原則 (ユーザー明示): **音作りは要望より実機 (Wurlitzer/Rhodes) 挙動の再現を優先**する。

| 指摘 | 結論・対応 |
| --- | --- |
| Wurli アタックが強すぎる | 実機リードは数ms かけて立ち上がり、振幅はほぼ単一指数で鳴る。Wurli プリセットを実機寄りに: carrierDecay 0.35→0.55s、knee 0.40→0.60 (アタック後の thump 解消)、attack 1→4ms、noiseBurst 0.25→0.16、barkBloom 0.62→0.42 |
| RELEASE が効かない/減衰一定 | **実機仕様につき意図的・正しい挙動**。実機 EP のリード/タイン減衰は物理固定の指数で、可変 release つまみは実機に存在しない。RELEASE はキーオフ後のダンパー (allowTailOff→State::Release) のみに作用。保持中の固定減衰 = Phase 9-1 の Decay2 (実機の物理減衰)。コード変更なし (バグではない) |
| MASTER を歪まない上限まで上げたい / DRIVE と併せ意図的歪み | 実機 Wurli は内蔵アンプを押すと歪む。`kMasterMaxGain=6.0` を新設し MASTER 0–100%→gain 0–6 にレンジ拡張。低中域はクリーンで大きく、DRIVE↑＋MASTER↑ で最終ソフトクリップ (knee 0.8→0.7) に到達しアンプ的オーバードライブ。±1.0 上限 (DAC 保護) は維持。MASTER 既定 60%、ヘッダ atomic 既定も 3.6 (=60%×6) に整合 |

> RELEASE の聴き方: 「押してすぐ離す」と「押し続ける」で比較。前者はダンパー (RELEASE) が、
> 後者は実機同様の固定物理減衰 (Decay2) が支配する。両者の違いが実機の挙動。

**Phase 9-4 完了 (リバーブ刷新 + Rhodes パントレモロ — 2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| リバーブ構成変更 | `juce::dsp::Reverb` を 100% wet 化し、自前で「プリディレイ → reverb → wet 高域ダンプ → dry にセンド加算」に再構成。Freeverb の箱鳴り/プリディレイ無しを解消 |
| プリディレイ | `juce::dsp::DelayLine` で 25ms。EP アタックを濁さずプレート的に後から鳴る |
| wet 高域ダンプ | wet センドに 1 次 LPF (~7kHz)。シューゲイザー的な暗いプレート bloom |
| reverb パラメータ | roomSize 0.75→0.82 (広い)、damping 0.45→0.60 (暗いテイル)、wet=1/dry=0 |
| REVERB ノブ | wet センドゲイン (0–0.6) に役割変更。`reverbDirty` の再構築機構は不要になり**完全削除** |
| Rhodes パントレモロ | 実機 Rhodes Suitcase の "vibrato" はステレオ**オートパン**。`currentPreset==Rhodes` 時は等パワーパン (gL²+gR²一定=レベル変動なし、定位のみ移動)。Wurlitzer は従来どおり振幅トレモロ (145 アンプ) |
| 実機根拠 | Suitcase 回路 = ステレオ強度パン / Wurli 200A = アンプの真の振幅トレモロ。両者を別処理に分岐 |

> 注: リバーブ/パンは base レート (OS 後) で処理。wet バッファは prepareToPlay で
> maxBlock 確保し再アロケーション無し。モノデバイス時は L のみ (R==null) で従来挙動。

**Phase 9-5 完了 (状態永続化 — 2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| 仕組み | `juce::ApplicationProperties` (PropertiesFile)。保存先 `~/Library/Application Support/MySynth/MySynth.settings` (XML) |
| 保存タイミング | `~MainComponent` (通常終了パス) で `saveState()`。preset + 全9ノブ + MASTER を書き出し |
| 復元 | 起動時、コンストラクタ末尾 `applyPreset()` 後に `loadState()`。保存プリセットを再適用しキーボードレンジ/トグルを設定後、各ノブを保存値で上書き (sendNotification で atomic も更新)。初回起動 (キー無し) はプリセット既定のまま |
| 順序ハザード回避 | `applyPreset` からは保存しない (構築中の上書き前に既定値で上書き保存されるのを防止) |
| 検証 | Rhodes+非デフォルト値を注入→起動→終了でラウンドトリップ実地確認済み (preset=1, drive=2.5, reverb=55, master=85 が保持) |
| 制約 (将来) | 通常終了で保存。クラッシュ時のセッションは失う。デバウンス自動保存/APVTS 移行は将来タスク。`juce_data_structures` を CMake に追加 |

**Phase 9-6 完了 (描画最適化 — 2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| 問題 | 旧: `startTimer(80)` 12fps で全面 `repaint()`。静的な木目/枠/文字まで毎ティック再ラスタライズ → "もっさり" |
| 対応 | `startTimer(33)` 30fps。timerCallback で **VU 帯と LED クラスタの dirty rect のみ・状態変化時のみ** `repaint(rect)`。JUCE が paint() をその矩形にクリップ → 静的描画はラスタ段でスキップ |
| 効果 | 再描画面積 ≒ 416k px → ~24k px (約17分の1) かつ 30fps で滑らか。`updateMidiStatus()` は 15ティック毎 (~0.5s) に間引き |
| 見た目 | **変更なし** (paint() 自体は不変、座標も不変)。プリセット変更時は applyPreset の全 repaint で nameplate 等も更新 |
| 設計判断 | 完全な子コンポーネント化は見送り。理由: LED クラスタ座標がプリセットボタン (ROADS) と z 順で重なっており、子化すると「ボタン背後で不可視」か「ボタンに被さる」かでビジュアル再設計 (ユーザー未依頼) が必要。dirty rect なら見た目不変で同等の性能を達成 |

> 既存の別課題 (本対応で新規発生したものではない): ヘッダの LED クラスタと
> ROADS プリセットボタンの矩形が一部重なる。将来の UI レイアウト見直しで整理。

**Phase 9-7 完了 (ノブ UX 標準化 — 2026-05-17):**

| 項目 | 内容 |
| --- | --- |
| ダブルクリック既定値復帰 | `setupKnob` で全ノブに `setDoubleClickReturnValue(true, def)`。プリセット系8ノブは `applyPreset` の `snap` で**現プリセット値**へ再ポイント (微調整を素早くプリセット基準に戻せる)。MASTER はグローバルゆえ構築既定 (60%) のまま |
| マウスホイール | 全ノブ `setScrollWheelEnabled(true)` 明示 |
| 微調整 | Cmd/Ctrl ドラッグで細かく動く (juce::Slider 組込。レビューの「shift」ではなく JUCE 標準の Cmd/Ctrl を採用) |
| loadState 整合 | 復元時は applyPreset→保存値で上書きの順。ダブルクリックはプリセット基準に戻る (直感的で一貫) |

> #10 後半 (ユーザープリセット保存/管理) は本ターン未実施。理由: ComboBox + Save As +
> ディレクトリ保存/削除/ファクトリ vs ユーザー + ヘッダ配置 (LED/ボタン重なり既知) は
> 1–2 日規模で UI 設計判断を伴う。セッション永続化 (#8) で日常用途は充足済みのため、
> 専用タスクとして設計合意の上で着手するのが適切と判断。

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
| Tremolo方式 | 振幅トレモロ (145アンプ) | ステレオ オートパン (Suitcase) |
| Tremolo | 5Hz / 28% | 4Hz / 12% |
| Reverb Wet (send) | 18% | 48% |

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
| **Phase 9** | **音作り(9-1〜9-4 ✅) / 永続化(9-5 ✅) / 描画(9-6 ✅) / ノブUX(9-7 ✅) / 残: プリセット管理・配布** | 進行中 |
