import {
  AbsoluteFill,
  interpolate,
  Sequence,
  staticFile,
  useCurrentFrame,
  useVideoConfig,
} from "remotion";
import { Audio, Video } from "@remotion/media";

export const REVERB_FPS = 30;
export const REVERB_WIDTH = 1360;
export const REVERB_HEIGHT = 884;
const INTRO_SECONDS = 10;
const DEMO_SECONDS = 86;
const OUTRO_SECONDS = 5;
const DEMO_START = INTRO_SECONDS * REVERB_FPS;
const OUTRO_START = DEMO_START + DEMO_SECONDS * REVERB_FPS;
export const REVERB_DURATION_FRAMES = OUTRO_START + OUTRO_SECONDS * REVERB_FPS;

const captions = [
  [0, 7, "DEFAULT  —  自然でバランスのよい標準設定"],
  [7, 13, "VOICE BOOTH  —  短く明瞭な声のための空間"],
  [13, 19, "SMALL WOOD ROOM  —  小さく暖かな木の部屋"],
  [19, 25, "DIALOGUE STAGE  —  台詞を邪魔しない控えめな響き"],
  [25, 31, "SOFT CHAMBER  —  暗く柔らかな親密さ"],
  [31, 37, "OPEN HALL  —  広く長い残響"],
  [37, 43, "TAIL ONLY  —  後期残響だけを試聴"],
  [43, 49, "ROOM BODY  —  初期反射と残響テール"],
  [49, 55, "MIX 100%  —  ウェット成分だけを確認"],
  [55, 61, "ER SOLO  —  初期反射だけを試聴"],
  [61, 67, "ROOM BODY  —  完成した部屋鳴りへ復帰"],
  [67, 73, "WET MONO ON  —  ウェット入力だけをモノラル化"],
  [73, 79, "WET MONO OFF  —  ステレオの広がりを保持"],
  [79, 86, "RESET  —  Defaultへ完全初期化"],
] as const;

const narration = [
  ["00-title.wav", 0.6, 2.05, undefined],
  ["00-intro.wav", 2.7, 6.7, undefined],
  ["01-booth.wav", 17.2, 4.5, 7.2],
  ["02-rooms.wav", 23.2, 6.0, 13.2],
  ["03-body.wav", 47.2, 5.4, 37.2],
  ["04-early.wav", 65.2, 4.5, 55.2],
  ["05-mono.wav", 77.2, 5.8, 67.2],
  ["06-reset.wav", 89.2, 3.8, 79.2],
] as const;

const sourceVolume = (frame: number) =>
  narration.reduce((volume, [, , duration, sourceStart]) => {
    if (sourceStart === undefined) return volume;
    const start = sourceStart * REVERB_FPS;
    const end = (sourceStart + duration) * REVERB_FPS;
    const fade = 0.16 * REVERB_FPS;
    return Math.min(
      volume,
      interpolate(frame, [start - fade, start, end, end + fade], [1, 0.3, 0.3, 1], {
        extrapolateLeft: "clamp",
        extrapolateRight: "clamp",
      }),
    );
  }, 1);

const Background: React.FC<{ children: React.ReactNode }> = ({ children }) => (
  <AbsoluteFill
    style={{
      background: "radial-gradient(circle at 50% 42%, #35271f, #171514 45%, #090909 82%)",
      color: "#f2e9dc",
      fontFamily: "Segoe UI, Meiryo, sans-serif",
      alignItems: "center",
      justifyContent: "center",
    }}
  >
    {[300, 500, 720, 960].map((size) => (
      <div key={size} style={{ position: "absolute", width: size, height: size, borderRadius: "50%", border: "1px solid rgba(214,149,98,.11)" }} />
    ))}
    {children}
  </AbsoluteFill>
);

const Intro: React.FC = () => {
  const frame = useCurrentFrame();
  const opacity = interpolate(frame, [0, 15], [0, 1], { extrapolateRight: "clamp" });
  return (
    <Background>
      <div style={{ zIndex: 1, textAlign: "center", opacity }}>
        <div style={{ color: "#e4a16f", fontSize: 70, fontWeight: 800, letterSpacing: ".08em" }}>NEKOSPACE REVERB</div>
        <div style={{ marginTop: 22, fontSize: 38, fontWeight: 650 }}>声に、自然な部屋の息づかいを。</div>
        <div style={{ marginTop: 46, fontSize: 27, lineHeight: 1.7 }}>初期反射と16ライン残響をひとつの操作画面へ</div>
        <div style={{ marginTop: 35, color: "rgba(242,233,220,.62)", fontSize: 19, letterSpacing: ".18em" }}>HEADPHONES RECOMMENDED</div>
      </div>
    </Background>
  );
};

const Caption: React.FC<{ text: string }> = ({ text }) => (
  <div style={{ position: "absolute", left: "50%", top: 70, transform: "translateX(-50%)", padding: "10px 22px", color: "#fff5e8", background: "rgba(17,15,14,.9)", border: "1px solid rgba(214,149,98,.85)", borderRadius: 8, fontFamily: "Segoe UI, Meiryo, sans-serif", fontSize: 25, fontWeight: 700, whiteSpace: "nowrap", boxShadow: "0 5px 20px rgba(0,0,0,.45)" }}>{text}</div>
);

const Outro: React.FC = () => (
  <Background>
    <div style={{ zIndex: 1, textAlign: "center" }}>
      <div style={{ color: "#e4a16f", fontSize: 58, fontWeight: 800, letterSpacing: ".08em" }}>NEKOSPACE REVERB</div>
      <div style={{ marginTop: 24, fontSize: 28 }}>Voice / Music / ASMR / Audio Drama</div>
      <div style={{ marginTop: 62, color: "rgba(242,233,220,.72)", fontSize: 20 }}>音声：VOICEVOX:猫使アル</div>
    </div>
  </Background>
);

export const NekoSpaceReverbDemo: React.FC = () => {
  const { fps } = useVideoConfig();
  return (
    <AbsoluteFill style={{ backgroundColor: "#0b0b0b" }}>
      <Sequence durationInFrames={DEMO_START}><Intro /></Sequence>
      <Sequence from={DEMO_START} durationInFrames={DEMO_SECONDS * fps}>
        <Video src={staticFile("NekoSpace_Reverb_demo_cut.mp4")} style={{ width: "100%", height: "100%" }} objectFit="contain" volume={(frame) => sourceVolume(frame)} />
      </Sequence>
      {captions.map(([from, to, text]) => (
        <Sequence key={text} from={DEMO_START + from * fps} durationInFrames={(to - from) * fps}><Caption text={text} /></Sequence>
      ))}
      {narration.map(([file, from, duration]) => (
        <Sequence key={file} from={Math.round(from * fps)} durationInFrames={Math.round(duration * fps)}>
          <Audio src={staticFile(`narration-reverb/${file}`)} volume={0.92} />
        </Sequence>
      ))}
      <Sequence from={OUTRO_START} durationInFrames={OUTRO_SECONDS * fps}><Outro /></Sequence>
    </AbsoluteFill>
  );
};
