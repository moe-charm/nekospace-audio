// SPDX-FileCopyrightText: 2026 charmpic
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// The manual, shown in a window you open on purpose.
//
// This replaced a footer line that updated on hover. That was worse than nothing: it
// moved in the corner of your eye every time the mouse crossed the window, whether or
// not you wanted help, and one line is not enough to explain something like Near Field.
// Help you ask for can be as long as it needs to be.
//
// A second language means a second column here, same as HelpText.h.
#include <juce_core/juce_core.h>
#include <vector>
#include "Language.h"

namespace nsbui
{
struct HelpSection { const char* heading; const char* body;
                     const char* headingJa; const char* bodyJa; };

inline std::vector<HelpSection> helpSections()
{
    return {
        { "What this is",
          "A binaural spatializer for voice. It takes a mono voice - or a stereo pair - "
          "and places it somewhere around the listener's head, including right up against "
          "an ear. It is built for audio drama and ASMR rather than for music panning, so "
          "the near-field behaviour and the way a close voice survives a room matter more "
          "here than the size of the reverb.",
          "これは何か",
          "声のためのバイノーラル空間化プラグイン。モノラルの声、またはステレオ素材を、聴き手の頭のまわりの任意の位置に置く。耳のすぐそばに置くこともできる。\n音楽のパンニングではなく音声作品とASMRのために作ってあるので、リバーブの規模より、近接時のふるまいと、部屋を足しても声が近いままでいられるかを重視している。" },

        { "Getting started",
          "Put it on a voice track, open the Presets menu, and try \"Left Ear 3 cm\". Then "
          "drag the orange dot in the pad: left and right move it around you, the scroll "
          "wheel changes distance, double-click returns it to the front at 1 m.\n"
          "Everything else refines that. If a control is not obvious, it is explained "
          "below.",
          "まず試す",
          "声のトラックに挿し、Presetsメニューから「Left Ear 3 cm」を選ぶ。あとはパッドのオレンジの点をドラッグする。左右に動かすと自分のまわりを回り、ホイールで距離が変わり、ダブルクリックで正面1mに戻る。\n他はすべてその微調整でしかない。分かりにくい操作子は以下で説明する。" },

        { "Position",
          "AZIMUTH is the direction around you - 0 straight ahead, positive to your right, "
          "and it wraps continuously past +/-180 so a source can circle you without a jump.\n"
          "DISTANCE is how far away. Below roughly 30 cm the near-field model takes over "
          "and the two ears start hearing genuinely different things.\n"
          "ELEVATION is height. Be aware it is the weak axis - see Known limits.\n"
          "WIDTH and SOURCE MODE only matter for stereo material: Mono Object folds the "
          "input down to a single source, Linked Stereo keeps left and right apart by "
          "the Width angle.",
          "位置",
          "AZIMUTH は自分のまわりの方向。0が正面、正の値が右。±180度をまたいでも連続なので、音源が跳ばずに一周できる。\nDISTANCE は距離。だいたい30cmを切ると近接モデルが主役になり、左右の耳が本当に別のものを聞きはじめる。\nELEVATION は高さ。ただしここは弱い軸なので「既知の限界」を読んでほしい。\nWIDTH と SOURCE MODE はステレオ素材のときだけ意味を持つ。Mono Object は入力を1つの音源にまとめ、Linked Stereo は左右をWidthの角度だけ離して置く。" },

        { "Near Field - the at-the-ear effect",
          "This is the control that makes a whisper feel like it is against your ear "
          "rather than merely panned hard left.\n"
          "At 0 % both ears are attenuated by the same distance, so the difference between "
          "them comes only from your head being in the way. That is what a conventional "
          "panner does, and it is the right setting for music, ambience and anything that "
          "should sit naturally in the mix.\n"
          "At 100 % each ear gets its own distance to the source, worked out along the "
          "actual path around the head. A source 12 cm from the left ear is then about "
          "24 dB louder on that side instead of 6 dB, and arrives noticeably earlier. That "
          "is the ear-whisper effect.\n"
          "It only does anything up close. At two metres the two settings are identical.",
          "Near Field — 耳元の効果",
          "囁きを「左に大きく振った音」ではなく「耳に触れている」と感じさせるのがこの操作子。\n0%では両耳が同じ距離で減衰するので、左右差は頭が邪魔をすることからしか生まれない。普通のパンナーと同じふるまいで、音楽・環境音・ミックスに自然に馴染ませたいものに向く。\n100%では左右の耳がそれぞれ自分の距離を持つ。頭のまわりを回り込む実際の経路で計算するので、左耳から12cmの音源は6dBではなく約24dB大きくなり、到達も目に見えて早くなる。これが耳元の効果。\n効くのは近いときだけ。2mも離れれば0%と100%は同じ音になる。" },

        { "Room",
          "ROOM sets how much of the room you hear. At 0 the output is exactly the dry "
          "binaural render, bit for bit.\n"
          "SIZE is how big the room sounds - its dimensions. Small is a booth, large is "
          "a hall.\n"
          "DECAY is how long it rings, and it is deliberately separate from SIZE. That "
          "separation is the point: a tiled bathroom is a small room with a long, bright "
          "tail, and you cannot build one if the two are welded together. Small and long "
          "is tile; large and short is a treated studio.\n"
          "DAMPING is how absorbent the surfaces are. Higher is a softer room: curtains, "
          "carpet, bedding. Lower is tile and glass.\n"
          "EARLY/LATE chooses what you hear of it. Early reflections are the room's shape "
          "and its size cues; late reverb is its tail. Each of the six reflections is "
          "rendered through the head model from the direction its image actually comes "
          "from, so a raised source really does get a floor bounce from below.",
          "Room（部屋）",
          "ROOM は部屋をどれだけ聞かせるか。0のとき出力はドライのバイノーラルとビット単位で一致する。\nSIZE は部屋の広さ、つまり寸法。小さければブース、大きければホール。\nDECAY は尻尾の長さで、SIZE とは意図的に切り離してある。ここが肝心で、タイル張りの浴室は「狭いのに長く明るく鳴る」部屋であり、両者が一体だと作れない。狭くて長ければタイル、広くて短ければ吸音した収録スタジオ。\nDAMPING は壁がどれだけ吸うか。高いほど柔らかい部屋 — カーテン、カーペット、寝具。低いとタイルやガラス。\nEARLY/LATE はそのうち何を聞くか。初期反射は部屋の形と広さの手がかり、後期残響は尻尾。6つの反射はそれぞれ、その像が実際に来る方向で頭部モデルを通してあるので、音源を上げれば床の反射がちゃんと下から返る。" },

        { "Voice Duck",
          "Reverb is what stops a close voice sounding close. Add enough room for the "
          "scene and the whisper you carefully placed 3 cm from an ear is suddenly a "
          "metre away.\n"
          "VOICE DUCK holds the late reverb down while the voice is speaking and lets it "
          "back up at the end of a phrase. The direct sound and the early reflections are "
          "never touched, so the room keeps its shape and the voice keeps its position; "
          "only the tail gets out of the way.\n"
          "The reverb is still being fed the whole time - it is held down, not switched "
          "off - so what you hear at the end of a line is a tail that has been building "
          "all along, not one starting from silence.\n"
          "DUCK REL is how long that takes. Short (150 ms) is tight and close; long "
          "(1 s+) lets the room bloom gradually after each line.\n"
          "There is no threshold to set. It measures the voice against its own recent "
          "level, so one setting works for a whisper and for a shout.",
          "Voice Duck",
          "近い声が近く聞こえなくなる原因はリバーブ。場面に必要なだけ部屋を足すと、耳から3cmに置いたはずの囁きが急に1m先へ行ってしまう。\nVOICE DUCK は声が鳴っている間だけ後期残響を押さえ、語尾で戻す。直接音と初期反射には一切触らないので、部屋の形も声の位置も保たれる。どくのは尻尾だけ。\nしかもリバーブには常に信号を送り続けている。止めているのではなく押さえているだけなので、語尾で聞こえるのは、ずっと裏で育っていた残響であって、無音から生えてくるものではない。\nDUCK REL はその戻りの速さ。150msなら締まって近く、1秒以上なら台詞のあとにゆっくり部屋がひらく。\nしきい値の設定は無い。声をその声自身の直近のレベルと比べるので、囁きにも叫びにも同じ設定で効く。" },

        { "HRTF Profile",
          "Which head model turns a direction into what your two ears hear.\n"
          "Analytic B is the default and the one to use.\n"
          "Analytic A is the original model, kept only so the difference can be heard.\n"
          "KU100 is measured from a real dummy head. It is experimental, needs a 48 kHz "
          "session, and is only present in development builds - at other rates the plugin "
          "quietly falls back to Analytic B and says so at the bottom of the window.\n"
          "Custom is whatever you dialled in with the Elevation Lab.",
          "HRTF Profile",
          "方向を「両耳に届く音」に変換する頭部モデルの選択。\nAnalytic B が初期値で、通常はこれを使う。\nAnalytic A は最初のモデル。違いを聴き比べるためだけに残してある。\nKU100 は実在のダミーヘッドの実測データ。実験的で、48kHzのセッションが必要、開発ビルドにしか入っていない。他のレートでは自動的にAnalytic Bに戻り、そのことが画面下に表示される。\nCustom は Elevation Lab で自分で追い込んだ設定。" },

        { "Elevation Lab",
          "A tuning bench for the height cue, because height is the part that depends most "
          "on your own ears and headphones.\n"
          "Four controls. UP and DOWN set how far a raised or lowered source departs from "
          "ear level, and they are independent - you can chase \"above\" without dragging "
          "\"below\" along with it. BODY is the shoulder reflection, a lower-frequency cue "
          "that survives headphone colouration better than the pinna notches do. FOCUS is "
          "how narrow the notch is: sharp colouring versus a broad tonal shift.\n"
          "1.00 on everything is exactly Analytic B, so Reset really resets. Work with the "
          "room off and real voice material. When something works, Copy as C++ puts the "
          "numbers on the clipboard.",
          "Elevation Lab",
          "高さの手がかりを耳で調整するための台。高さは、自分の耳とヘッドフォンにもっとも左右される部分だから。\n操作子は4つ。UP と DOWN は上・下の音が耳の高さからどれだけ離れるかで、互いに独立している。「上」を追い込んでも「下」は動かない。BODY は肩の反射で、耳たぶのノッチより低い帯域にあるぶんヘッドフォンの色付けに強い。FOCUS はノッチの鋭さ — 細い色付けか、広い音色変化か。\nすべて1.00がAnalytic Bそのものなので、Resetは本当に元に戻る。部屋を切って、実際に使う声で作業するとよい。良い設定が見つかったら Copy as C++ で数値を取り出せる。" },

        { "Quality and Output",
          "QUALITY: Standard uses the full head response. Economy halves it and costs "
          "roughly half the CPU; the difference is small on a voice.\n"
          "OUTPUT is a trim before the safety limiter. The limiter exists because the "
          "near-field gain can reach +32 dB at the skull - GR on the meter shows how hard "
          "it is working. If GR is lit most of the time, pull OUTPUT down.\n"
          "ROOM BYPASS mutes the room instantly for an A/B against the dry render.\n"
          "The plugin reports 2 ms of latency, which is real and gets compensated by the "
          "host. Bypassing it keeps the same delay, so switching does not shift timing.",
          "Quality と Output",
          "QUALITY: Standard は頭部応答を全長で使う。Economy は半分にしてCPUを約半分にする。声ならその差は小さい。\nOUTPUT はセーフティリミッターの手前のトリム。リミッターがあるのは、近接時のゲインが頭に触れる距離で+32dBに達しうるため。メーターのGRがその働き具合を示す。GRが点きっぱなしならOUTPUTを下げる。\nROOM BYPASS は部屋を即座に消してドライと比較するためのもの。\nレイテンシは2msと報告される。これは実在する遅延でホストが補正する。バイパス中も同じ遅延を保つので、切り替えてもタイミングはずれない。" },

        { "Recipes",
          "A whisper at the left ear: Left Ear 3 cm preset, Near Field 100 %, Room around "
          "10 %, Voice Duck 60 %.\n"
          "A voice in a room that still sounds close: Distance 0.8-1.2 m, Room 35-45 %, "
          "Early/Late around 30 %, Voice Duck 50-70 %, Duck Rel 400 ms.\n"
          "Ambience or music that should sit naturally: Near Field 0 %, Linked Stereo, "
          "Width 60-90 deg, Voice Duck 0 %.\n"
          "Someone circling the listener: automate Azimuth. It wraps cleanly and the "
          "filters crossfade, so a full circle has no seam.",
          "設定例",
          "左耳の囁き: Left Ear 3 cm プリセット、Near Field 100%、Room 10%前後、Voice Duck 60%。\n部屋にいるのに近いままの声: Distance 0.8〜1.2m、Room 35〜45%、Early/Late 30%前後、Voice Duck 50〜70%、Duck Rel 400ms。\n自然に馴染ませたい環境音や音楽: Near Field 0%、Linked Stereo、Width 60〜90度、Voice Duck 0%。\n聴き手のまわりを回る音: Azimuth をオートメーションする。境界で跳ばずフィルターもクロスフェードするので、一周しても継ぎ目は出ない。" },

        { "Known limits",
          "Elevation is weak. It makes a real, audible change and a mild sense of vertical "
          "movement, but it is not a dependable \"that is above me\" cue. Four approaches "
          "were built and measured, each improved the numbers, and none produced a "
          "decisive percept. That is the ceiling of static binaural played to arbitrary "
          "listeners: the spectral cues that carry height depend on the shape of your own "
          "pinnae, and headphones colour exactly the band those cues live in. Use "
          "elevation as a colour and let the script, the footsteps and the room carry "
          "height.\n"
          "The measured KU100 profile is 48 kHz only and is not bundled.\n"
          "Windows only so far. No user SOFA import yet.",
          "既知の限界",
          "高さは弱い。音は確かに変わるし多少の上下動も感じられるが、「上にいる」と確信できる手がかりにはなっていない。4つの手法を実装して計測し、どれも数値は改善したが、どれも決定的な知覚を生まなかった。\nこれは不特定多数に配信する静的バイノーラルの限界である。高さを運ぶスペクトルの手がかりは聴く人自身の耳たぶの形に依存し、ヘッドフォンはまさにその帯域を色づけする。高さは演出の色付けとして使い、台詞・足音・部屋の響きに運ばせるのがよい。\n実測KU100プロファイルは48kHz限定で、配布物には含まれない。\n現時点ではWindowsのみ。ユーザーのSOFA読み込みは未対応。" },

        { "Licence",
          "Free software under the GNU Affero General Public License v3.0 or later.\n"
          "Copyright (C) 2026 charmpic. NekoSpace Audio.\n"
          "The audio you make with this is yours. The licence covers the plugin's own "
          "source and binaries; a rendered file is not a derivative work of the software, "
          "so recordings and audio dramas made with it carry no obligation.\n"
          "There is NO WARRANTY, to the extent permitted by law. You may redistribute it "
          "under the terms of the licence; the full text ships as LICENSE alongside this "
          "plugin and is at gnu.org/licenses/agpl-3.0.html.\n"
          "Source: github.com/moe-charm/nekospace-audio",
          "ライセンス",
          "GNU Affero General Public License v3.0 以降のフリーソフトウェア。\nCopyright (C) 2026 charmpic. NekoSpace Audio.\nこれで作った音声は君のもの。ライセンスが対象にするのはプラグイン自身のソースとバイナリで、書き出したファイルはソフトウェアの派生物ではない。このプラグインで制作した録音や音声作品に義務は一切かからない。\n法律が許す範囲で無保証。ライセンスの条件のもとで再配布できる。全文はこのプラグインに同梱の LICENSE、および gnu.org/licenses/agpl-3.0.html にある。\nソース: github.com/moe-charm/nekospace-audio" },
    };
}
} // namespace nsbui
