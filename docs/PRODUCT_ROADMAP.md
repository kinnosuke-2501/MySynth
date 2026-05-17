# Product Quality Roadmap — Electric Piano Synth

策定日: 2026-05-17 / 最終更新: 2026-05-17 (Phase 8 完了時点)  
対象: macOS Standalone JUCE C++ 音源 (Wurlitzer / Rhodes)  
目標: Arturia V Collection レベルのプロダクト品質・UI/UX

> **最新状況 (2026-05-17):** Phase 6 (音色) / Phase 7 (UI/UX 全面リデザイン) 完了。
> レビューに基づく「正しさ三点パック」(位相ラップ・DCブロッカー・ウィンドウ固定サイズ)
> と **ボイス生成の 2× オーバーサンプリング** (Phase 8) を実装済み。
> 本ロードマップの旧 Phase 6/7 記述・「ビンテージ感ゼロ/固定700×420」等は達成済みのため
> §1 と §5 を改訂。詳細実装ログは `SESSION_CONTEXT.md` を参照。

---

## 1. 現状評価

### 達成済み (Phase 1–5 + Phase 6 一部)

| カテゴリ | 内容 | 評価 |
|---|---|---|
| 音声合成 | 2-op FM + ノイズバースト + ステレオデチューン | 良好 |
| アンチエイリアス | ボイス生成の 2× オーバーサンプリング (Phase 8) | 良好 |
| 信号の正しさ | 位相ラップ + DC ブロッカー (Phase 8) | 良好 |
| エフェクト | Chorus → Tremolo → Reverb チェーン | 良好 (リバーブ品質は要刷新) |
| プリセット | Wurlitzer 200A / Rhodes Mark II | 基本実装済み |
| UI | VintageLookAndFeel + 800×520 + 8ノブ + VU メーター (Phase 7) | プロ水準の土台あり |
| MIDI | 全デバイス自動オープン / サステインペダル / pitch bend | 良好 (ホットプラグ未対応) |
| スレッド安全性 | atomic + midiCollector | 良好 |
| 音色ブラッシュアップ | 奇数倍音、buzz、velocity S字、afterglow、resonance、pedal noise | Phase 6 完了 |

### 未達・課題 (Phase 8 完了時点)

| カテゴリ | 問題 | 優先度 |
|---|---|---|
| ~~振幅エンベロープ~~ | ✅ Phase 9-1 完了: EpAmpEnvelope (2段指数ディケイ) でフラットsustain廃止 | 済 |
| ~~出力安全性~~ | ✅ Phase 9-2 完了: MASTER ノブ + 出力ソフトリミッタ | 済 |
| **空間系** | Freeverb は箱鳴り。プリディレイ/トーン無し。Rhodesはパントレモロ未対応 (ドリームポップ狙いの要) | P0 |
| **状態永続化** | 起動ごとにパラメータがリセットされる | P1 |
| **プリセット管理** | 保存・読み込み・ユーザープリセット なし | P1 |
| **MIDI** | ホットプラグ非対応 (起動後の接続無効) | P1 |
| **ピッチベンド** | 実装済み。UI からのレンジ変更は未対応 | P1 |
| **描画効率** | 12fps 全面 repaint。VU/LED を独立コンポーネント化し 30fps 部分描画へ | P1 |
| **ノブ UX** | ダブルクリック既定値復帰 / shift 微調整 / MIDIラーン 未設定 | P2 |
| **アクセシビリティ** | `g.drawText` のシルクスクリーン文字が VoiceOver 不可視 | P2 |
| **フォント** | システムフォント依存 (未埋め込み) | P2 |
| **配布** | 未署名・未公証 (Gatekeeper ブロック)、製品名/ID/アイコン未整備 | P2 |
| **レイアウト負債** | `paint`/`resized` の幾何二重計算 (固定サイズ化で実害は消失、整理は将来) | P3 |

---

## 2. 参考プロダクト分析

### 競合・目標水準

| 製品 | 価格帯 | 特徴 | 学ぶべき点 |
|---|---|---|---|
| Arturia Wurli V | $99 | 物理モデリング (Phi)、11+エフェクト、ベロシティカーブエディタ | 音色の深さ、UIのデザイン言語 |
| Arturia Rhodes V | $99 | 4フィルタースロット、ゾーン分割 | 操作系の洗練度 |
| Dexed (無料) | $0 | DX7完全再現、MIDI import、1000+プリセット | 機能の網羅性 |
| TAL-U-NO-62 | $59 | ビンテージ筐体モック、アニメーション | ビジュアルクオリティ |
| Surge XT (無料) | $0 | 全パラメータ MIDI CC可能、モジュレーション行列 | プロ機能の参考 |

### 本プロダクトのポジショニング

- **ターゲット**: ドリームポップ/シューゲイザー系プロデューサー、宅録ミュージシャン
- **差別化**: Wurlitzer + Rhodes に特化した「すぐ鳴る」シンプルさ + ビンテージ質感
- **非目標**: DAWプラグイン化、汎用シンセ化

---

## 3. 改善ロードマップ

### Phase 6: 音色ブラッシュアップ (P0 — 最優先)

**目標**: "それっぽい" から "あ、これだ" の音質へ

進捗メモ (2026-05-17):
- 6-1 の初回実装として、キャリアに 3次・5次倍音を追加
- 高音域 (C5以上) で modDecay を短くし、アタックの明るさを補正
- ベロシティ→音色は固定二乗から S字カーブへ変更
- 6-2 の初回実装として、Wurlitzer 系の reed clipping を模した buzz 段を追加
- 6-3 の初回実装として、pitch bend を ±2 半音で反映
- 6-4 の初回実装として、Rhodes の note-off afterglow を追加
- 6-4 の第2段として、Rhodes のサステインペダル中 sympathetic resonance を追加
- 6-4 の第3段として、サステインペダル down/up に mechanical pedal noise を追加
- 次の着手点は Wurlitzer / Rhodes の質感追い込み、または Phase 7 UI/UX

#### 6-1. 倍音構造の改善

現状の問題: 2-op FM だけでは Wurlitzer 特有の奇数倍音の鋸歯状クリップが再現できていない。

実装内容:
```
1. キャリア波形に奇数倍音を追加 (3次・5次 partials)
   - Wurlitzer: 3rd harmonic 約10%, 5th 約4% (実機測定値)
   - Rhodes: 3rd 約5% のみ (tine は sine に近い)

2. 高音域 (C5以上) のブライトネス補正
   - 現状: keyScaleCoeff で単純なFM深度スケーリング
   - 改善: 高音域で modDecay を短くし、attack transient を強調

3. ベロシティ → 音色の非線形マッピング
   - 現状: velPow=2 の固定二乗カーブ
   - 改善: 低速域は柔らかい、中速域で急激に明るくなる S字カーブ
```

#### 6-2. Wurlitzer 特有の "buzz" 再現

```cpp
// SynthVoice.cpp: キャリア波形にリードのクリッピング模擬を追加
// 現状: carrierL = sin(angle + mod)
// 改善: ソフトクリップ後に奇数倍音フィルタリング

float reedBuzz(float x, float driveAmt) {
    // 段階的ソフトクリップ: Wurlitzer リードの磁気飽和
    float clipped = std::tanh(x * (1.0f + driveAmt * 2.0f)) / std::tanh(1.0f + driveAmt * 2.0f);
    return clipped;
}
```

#### 6-3. ピッチベンド実装

```cpp
// ElectricPianoVoice に pitchWheelMoved() を実装
void pitchWheelMoved(int newPitchWheelValue) override {
    // ±2半音のデフォルト、UI で設定可能
    double semitones = (newPitchWheelValue - 8192) / 8192.0 * 2.0;
    pitchBendRatio = std::pow(2.0, semitones / 12.0);
}
```

#### 6-4. ペダルノイズ / 残響改善

- Rhodes の「bell afterglow」: ノートオフ後の高次倍音残響をシミュレート
- サステインペダルを踏んでいる間の sympathetic resonance (共鳴) 追加

---

### Phase 7: UI/UX 全面リデザイン (P0)

**目標**: ビンテージ楽器の質感 + 現代的な使いやすさ

#### 7-1. ビジュアルデザイン方針

```
コンセプト: "70年代製造の電子ピアノがデスクに置かれている"

カラーパレット:
  - 本体: ウォームブラック (#1C1B18) — 本革/艶消し金属
  - パネル: ダークウォルナット (#2D2416) — 木製サイドパネル
  - アクセント: アンバーゴールド (#D4860A) — 真鍮ノブ
  - テキスト: クリームホワイト (#F0EDE4) — スクリーン印刷文字
  - LED: アンバーオレンジ (#FF8C00) — 発音中インジケータ

フォント:
  - タイトル: 太字 sans-serif (ヴィンテージロゴ風)
  - ラベル: 細身 monospace (パネルシルクスクリーン風)
  - 数値: 7-seg display 風フォント or プレーン monospace
```

#### 7-2. レイアウト再設計 (800×520)

```
┌─────────────────────────────────────────────────────────────────┐
│  ╔══════════════════╗                            [○] [○] [●]    │
│  ║  ELECTRIC PIANO  ║   [WURLITZER]  [RHODES]    LED×3         │
│  ╚══════════════════╝                                           │
│  ─────────────────────────────────────────────────────────────  │
│                    ┌─── SOUND ──────────────────────────┐       │
│  FM DEPTH  ATTACK  RELEASE  DRIVE    │  TREMOLO  REVERB  CHORUS │
│  [knob]   [knob]  [knob]   [knob]   │  [knob]   [knob]  [knob] │
│                                     │                           │
│  ─────────────────────────────────────────────────────────────  │
│  [━━━━━━━━━━━━━━━━━━━━━━━━ VU METER ━━━━━━━━━━━━━━━━━━━━━━━]   │
│                                                                 │
│  [Panic]  [◁◁] [▷] [PRESET NAME ▼]              [Settings...]  │
└─────────────────────────────────────────────────────────────────┘

  ● 発音中 LED (アンバー)
  ● サステインペダル LED (ブルー)  
  ● CPU 負荷 LED (グリーン→レッド)
```

#### 7-3. カスタム LookAndFeel

JUCE のデフォルト LookAndFeel を継承したカスタムクラスを実装:

```cpp
// Source/VintageLookAndFeel.h
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // ロータリーノブ: 真鍮製ノブの描画
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    // ボタン: テキストボタンをハードウェアボタン風に
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool, bool) override;

    // カスタムフォント (JUCEのBinaryData経由でフォント埋め込み)
    juce::Font getLabelFont(juce::Label&) override;
};
```

#### 7-4. VU メーター実装

```cpp
// Source/VUMeter.h: リアルタイム RMS レベル表示
// AudioCallback から atomic<float> に書き込み、Timerで定期描画
// デザイン: アンバーの連続グラデーションバー (VU針風)
```

#### 7-5. リサイズ対応

```cpp
// setResizable(true, true) を有効化
// resized() 内を比率ベースレイアウトに変更
// 最小サイズ: 600×400 / 最大: 1200×750
```

---

### Phase 8: 機能拡張 (P1)

#### 8-1. 状態永続化 (APVTS導入)

現状のパラメータ管理 (`SynthParams` + atomic) を `AudioProcessorValueTreeState` に移行:

```cpp
// メリット:
// - 全パラメータを XML として自動保存/読み込み
// - MIDI CC オートメーション対応の基盤
// - undo/redo 対応の基盤

// 実装手順:
// 1. AudioProcessor ベースに移行 (現在は AudioAppComponent)
//    OR juce::AudioAppComponent のまま ValueTreeState を手動管理
// 2. 各パラメータを AudioParameterFloat として登録
// 3. getStateInformation() / setStateInformation() を実装
```

> **注意**: AudioAppComponent → AudioProcessor 移行は大きなリファクタリング。
> まずは手動の PropertiesFile (juce::ApplicationProperties) で起動時復元を実装し、
> APVTS移行は将来タスクとする。

#### 8-2. プリセット管理システム

```
設計:
  - Factory Presets: Wurlitzer 200A / Rhodes Mark II / Suitcase 73 / Stage 88
  - User Presets: ユーザーがノブ設定を名前付きで保存
  - 保存先: ~/Library/Application Support/ElectricPiano/Presets/

UI:
  - プリセット名ドロップダウン (現在のボタン方式を拡張)
  - 「Save as...」「Delete」ボタン
  - 前後ナビゲーション (◁◁ ▷ ボタン)
```

#### 8-3. MIDI ホットプラグ対応

```cpp
// 現状: 起動時に openAllMidiInputs() を一度だけ呼ぶ
// 改善: timerCallback() (200ms周期) 内でデバイスリストを比較し、
//       差分があれば新規デバイスをオープン、切断されたものをクローズ

void MainComponent::timerCallback()
{
    refreshMidiDevicesIfNeeded(); // デバイスリスト変化を検出
    updateMidiStatus();
    repaint();
}
```

#### 8-4. マスターボリューム

```cpp
// UIにマスターボリュームノブ追加 (0–100%, デフォルト 70%)
// getNextAudioBlock の最後で全サンプルに掛け算
// atomic<float> masterGain で UI→オーディオスレッド通信
```

---

### Phase 9: 配布・ポリッシュ (P2)

#### 9-1. アプリアイコン設計

```
コンセプト: Wurlitzer 200A の鍵盤断面図を抽象化したアイコン
サイズ: 1024×1024 PNG → icns に変換
色: アンバーゴールドとウォームブラックのグラデーション

CMakeLists.txt:
  juce_add_gui_app(MySynth
      ICON_BIG   "${CMAKE_SOURCE_DIR}/Assets/icon_1024.png"
      ICON_SMALL "${CMAKE_SOURCE_DIR}/Assets/icon_512.png"
  )
```

#### 9-2. コード署名・公証 (macOS Gatekeeper 対応)

```bash
# 前提: Apple Developer Program 登録 ($99/year)

# 署名
codesign --force --deep --sign "Developer ID Application: Your Name (XXXXXXXX)" \
  "build/MySynth_artefacts/Release/MySynth.app"

# 公証
xcrun notarytool submit "MySynth.zip" \
  --apple-id "your@email.com" \
  --team-id "XXXXXXXX" \
  --password "@keychain:notarytool-password"

# ステープル
xcrun stapler staple "MySynth.app"
```

#### 9-3. 製品名・バンドルID 更新

```cmake
# CMakeLists.txt
juce_add_gui_app(ElectricPiano
    PRODUCT_NAME  "Electric Piano"
    COMPANY_NAME  "Your Studio Name"
    BUNDLE_ID     "com.yourstudio.electricpiano"
    VERSION       "1.0.0"
)
```

---

## 4. 技術的改善事項

### パフォーマンス最適化

| 項目 | 現状 | 改善 |
|---|---|---|
| 角度リセット | なし (double が増大し続ける) | fmod または if (angle > 2π) angle -= 2π |
| `std::sin` コスト | サンプル毎に呼び出し | JUCE の `dsp::FastMathApproximations::sin` 活用 |
| Reverb パラメータ | リビルドあり | `reverbDirty` フラグは良い設計、維持 |

### コード品質

```
- SynthVoice.cpp の renderNextBlock: angle wrapping を追加すること
  (現状 double は long-running で精度劣化の可能性)
  
- MainComponent::handleIncomingMidiMessage: note-on with vel=0 の
  カウント処理にバグあり (note-on判定後に再びvel==0チェックしている)
  → isNoteOn()/isNoteOff() の判定順序を整理

- getNextAudioBlock 内の driveNorm 変数: (void)driveNorm でサプレス
  しているが未使用 — 削除するか実際に使用すること
```

### スレッド安全性の強化

```
- chorusMixAmt は atomic<float> だが、lastChorusMix は通常 float。
  これはオーディオスレッドのみが読み書きするので問題ないが、
  初期値の整合性に注意 (現状 -1.0f で初回更新を強制 — OK)

- currentSampleRate も double で非 atomic。
  prepareToPlay はメッセージスレッドから呼ばれるため、
  getNextAudioBlock との競合は実際には起きにくいが、
  atomic<double> にすることを推奨
```

---

## 5. 実装優先順位 (アクションアイテム)

### ✅ 完了済み (Phase 6–8)

| 旧# | タスク | 状態 |
|---|---|---|
| 1 | `renderNextBlock` の angle wrapping | ✅ Phase 8 (位相ラップ) |
| 2 | `driveNorm` 未使用変数 | ✅ 解消済み (現コードに存在せず) |
| 3 | ピッチベンド実装 | ✅ Phase 6-3 |
| 4 | `tanh` ベース reedBuzz | ✅ Phase 6-2 |
| 7 | `VintageLookAndFeel` クラス | ✅ Phase 7 |
| 8 | 800×520 化 | ✅ Phase 7 (固定サイズで確定 — Phase 8) |
| 9 | VU メーター | ✅ Phase 7 |
| 10 | LED デザイン改善 | ✅ Phase 7 |
| 12 | 倍音構造改善 (3/5次) | ✅ Phase 6-1 |
| — | DC ブロッカー / 2× オーバーサンプリング | ✅ Phase 8 (レビュー由来) |

### 次に着手すべき (レビュー由来・優先順)

| # | トラック | タスク | ファイル | 工数 |
|---|---|---|---|---|
| ~~5~~ | 音 | ✅ Phase 9-1 完了: 2段指数ディケイ (EpAmpEnvelope) | SynthVoice.h/cpp | 済 |
| ~~6~~ | 音 | ✅ Phase 9-2 完了: MASTER ノブ + 出力ソフトリミッタ | MainComponent.h/cpp | 済 |
| 7 | 音 | リバーブ刷新 (プリディレイ/高域ダンプ) + Rhodes パントレモロ ← **次** (ユーザー在席時) | MainComponent.cpp | 1–2日 |
| 8 | UX | 状態永続化 (ApplicationProperties) | MainComponent.cpp | 0.5日 |
| 9 | UX | VU/LED を独立コンポーネント化し 30fps 部分描画 | MainComponent.* | 0.5日 |
| 10 | UX | ノブ UX 標準化 (既定値復帰/微調整) + プリセット保存 | MainComponent.* | 1–2日 |
| 11 | 配布 | 製品名/Bundle ID/アイコン/署名・公証 | CMakeLists.txt 他 | 1日 + Apple Developer |

### 将来 (任意・低優先)

| # | タスク | 工数 |
|---|---|---|
| 12 | MIDI ホットプラグ対応 | 3h |
| 13 | 比率ベースレイアウト化 (リサイズ復活させる場合) | 1日 |
| 14 | アクセシビリティ (VoiceOver) / フォント埋め込み | 1日 |
| 15 | モジュレータ・フィードバック (DX系 EP の噛み付き) | 0.5日 |

---

## 6. "プロダクトらしさ" チェックリスト

起動体験:
- [ ] アイコンがプロダクト固有のデザイン
- [ ] 起動時に前回のプリセット・パラメータが復元される
- [ ] MIDI デバイスが自動検出・表示される
- [ ] 音が出るまで 3 秒以内

音の質:
- [ ] pp (ベロシティ=20) で暗く、ff (ベロシティ=120) で明るい
- [ ] キーを素早くリリースしても自然に減衰する
- [ ] ピッチベンドが滑らかに機能する
- [ ] 16 ノート同時発音で CPU 使用率 < 15% (M1 Mac基準)

UI/UX:
- [ ] ノブをドラッグしたとき値が即座に反映される (ジッター・クリックなし)
- [ ] ウィンドウをリサイズしてもレイアウトが崩れない
- [ ] VU メーターが音量に追従している
- [ ] プリセット切り替えで音が途切れない (allNotesOff + tail-off)

配布:
- [ ] 未署名警告が出ない (Gatekeeper 通過)
- [ ] `/Applications` にドラッグ&ドロップで動く
- [ ] macOS 13/14/15 で動作確認済み

---

## 7. 参考リソース

### 技術リファレンス

- [JUCE API: LookAndFeel_V4](https://docs.juce.com/master/classLookAndFeel__V4.html) — カスタム描画の起点
- [JUCE Tutorial: Custom LookAndFeel](https://docs.juce.com/master/tutorial_look_and_feel_customisation.html)
- [JUCE Tutorial: AudioProcessorValueTreeState](https://docs.juce.com/master/tutorial_audio_processor_value_tree_state.html)
- [Surge XT ソースコード](https://github.com/surge-synthesizer/surge) — プロ品質OSSシンセの参考
- [Dexed ソースコード](https://github.com/asb2m10/dexed) — FM シンセのプロ実装参考

### ビジュアルデザイン参考

- Arturia Wurli V のスクリーンショット — パネルデザイン
- TAL-U-NO-62 のUI — ビンテージ筐体の描画手法
- [The Plugin Lab](https://thepluginlab.com/) — オーディオプラグイン UI ギャラリー

### macOS 配布

- [Apple Developer: Notarizing macOS Software](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution)
- [JUCE: macOS Deployment](https://docs.juce.com/master/tutorial_app_basics.html)
