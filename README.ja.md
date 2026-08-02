# NekoSpace Audio

English: [README.md](README.md)

音声作品・ボイス制作向けのオーディオプラグインにゃ。C++17 / JUCE / CMake、AGPLv3にゃ。

| 製品 | 状態 | |
| --- | --- | --- |
| **NekoSpace Binaural** | 開発中 | 声に特化した3Dバイノーラル空間化 — [readme](plugins/binaural/README.md) |
| NekoSpace Reverb | 予定 | |
| NekoSpace Room | 予定 | |
| NekoSpace Delay | 予定 | |

## 現状 — 正直版

未リリースにゃ。パラメーターIDとプラグインコードは確定してるけど、音はまだ固まってないにゃ。

**NekoSpace Binaural は動くにゃ。** 左右・前後・距離、そして耳元の「耳のすぐそば」表現は
どれも宣伝どおりに機能するにゃ。JUCE非依存の受け入れテスト30本と
`pluginval --strictness-level 10` で検証済みにゃ。

**弱いのは上下にゃ。** 音は確かに変わるし、多少の上下動は感じられるけど、
「上にいる」と確信できる手がかりにはなってないにゃ。これは見つけられてないバグではなく、
**静的・非個人化バイノーラルの原理的な限界**にゃ。高さを運ぶスペクトル手がかりは
**聴く人自身の耳たぶの形**に依存するし、ヘッドフォンはまさにその 5〜12 kHz を色づけするにゃ。

4つの手を試して計測したにゃ — 数式モデルの再設計、KU100の実測データ、
胴体反射＋初期反射のHRTFレンダリング、そして聴く人ごとの手動調整にゃ。
**どれも数値は改善したけど、どれも決定的な知覚は生まなかった**にゃ。
上下は「演出の色付け」として使い、物語の要には据えないのが正解にゃ。

## ビルド

CMake 3.22以上、C++17コンパイラ（WindowsならVisual Studio 2022）、gitが必要にゃ。
JUCEは初回configure時に自動取得されるにゃ。

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

全製品がトップレベルのconfigure 1回でビルドされて、JUCEは1度だけ取得して共有されるにゃ。

## 構成

```
nekospace-audio/
├─ plugins/          製品ごとに1ディレクトリ、自己完結
│  └─ binaural/      src, tests, docs, tools, resources
├─ shared/           複数プラグインが実際に必要としたコードだけ
├─ docs/             製品横断の契約
└─ cmake/            ビルド補助
```

何をどこに置くか、特に **なぜ `shared/` を空で始めるのか** は
[docs/repo-layout.md](docs/repo-layout.md) にゃ。

## 全製品に共通のドキュメント

- [identity.md](docs/identity.md) — 著作権者とブランドの区別、リリース後は変更不可のVST3/AUコード
- [state-format.md](docs/state-format.md) — ホストのプロジェクトに何を保存するか、古いプロジェクトを読み続けるためのルール
- [realtime-contract.md](docs/realtime-contract.md) — スレッド規約
- [third-party-licenses.md](docs/third-party-licenses.md) — 依存ライセンスのSSOT

## ライセンス

**GNU Affero General Public License v3.0 or later** のフリーソフトウェアにゃ
（[LICENSE](LICENSE) 参照）。

    Copyright (C) 2026 charmpic

**このプラグインで作った音声は君のものにゃ。** AGPLが対象にするのはプラグイン自身の
ソースとバイナリにゃ。レンダリングされた音声はソフトウェアの派生物ではないにゃ
（LICENSE 第2条参照）。NekoSpaceで制作した録音や音声作品にAGPLの義務は一切かからないにゃ。
