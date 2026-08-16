import {
  AbsoluteFill,
  interpolate,
  Sequence,
  staticFile,
  useCurrentFrame,
  useVideoConfig,
} from "remotion";
import { Audio, Video } from "@remotion/media";

export const VIDEO_FPS = 30;
export const VIDEO_WIDTH = 1494;
export const VIDEO_HEIGHT = 908;

const INTRO_SECONDS = 12;
const DEMO_SECONDS = 57.125;
const OUTRO_SECONDS = 4;
const DEMO_START_FRAME = Math.round(INTRO_SECONDS * VIDEO_FPS);
const DEMO_DURATION_FRAMES = Math.round(DEMO_SECONDS * VIDEO_FPS);
const OUTRO_START_FRAME = DEMO_START_FRAME + DEMO_DURATION_FRAMES;
export const VIDEO_DURATION_FRAMES =
  OUTRO_START_FRAME + Math.round(OUTRO_SECONDS * VIDEO_FPS);

type Caption = {
  fromSourceSeconds: number;
  toSourceSeconds: number;
  text: string;
};

// These markers follow the private operation recording. They remain relative to
// the source so changing the title-card length never shifts the edit by hand.
export const CAPTIONS: Caption[] = [
  { fromSourceSeconds: 0, toSourceSeconds: 3.8, text: "音源を左へ：左耳側に定位" },
  { fromSourceSeconds: 4.3, toSourceSeconds: 13.8, text: "音源を右へ：右耳側に定位" },
  { fromSourceSeconds: 14.2, toSourceSeconds: 19, text: "Elevation +58°：上方向の音像" },
  { fromSourceSeconds: 19.5, toSourceSeconds: 33.5, text: "Elevation −50°：下方向の手がかり" },
  { fromSourceSeconds: 34, toSourceSeconds: 56.5, text: "Distance 86 cm：耳元へ近づける" },
];

type Narration = {
  file: string;
  fromSeconds: number;
  durationSeconds: number;
  fromSourceSeconds?: number;
};

export const NARRATION: Narration[] = [
  { file: "narration/00-title.wav", fromSeconds: 0.6, durationSeconds: 2.05 },
  { file: "narration/00-intro.wav", fromSeconds: 2.8, durationSeconds: 8.81 },
  { file: "narration/01-left.wav", fromSeconds: INTRO_SECONDS + 0.2, fromSourceSeconds: 0.2, durationSeconds: 3.52 },
  { file: "narration/02-right.wav", fromSeconds: INTRO_SECONDS + 4.6, fromSourceSeconds: 4.6, durationSeconds: 3.44 },
  { file: "narration/03-above.wav", fromSeconds: INTRO_SECONDS + 14.4, fromSourceSeconds: 14.4, durationSeconds: 3.53 },
  { file: "narration/04-below.wav", fromSeconds: INTRO_SECONDS + 19.8, fromSourceSeconds: 19.8, durationSeconds: 3.2 },
  { file: "narration/05-near.wav", fromSeconds: INTRO_SECONDS + 34.2, fromSourceSeconds: 34.2, durationSeconds: 4.72 },
];

const sourceVolumeAtFrame = (frame: number, fps: number) =>
  NARRATION.reduce((volume, narration) => {
    if (narration.fromSourceSeconds === undefined) return volume;
    const start = narration.fromSourceSeconds * fps;
    const end = (narration.fromSourceSeconds + narration.durationSeconds) * fps;
    const fade = 0.16 * fps;
    const duck = interpolate(
      frame,
      [start - fade, start, end, end + fade],
      [1, 0.28, 0.28, 1],
      { extrapolateLeft: "clamp", extrapolateRight: "clamp" },
    );
    return Math.min(volume, duck);
  }, 1);

const OverlayText: React.FC<{
  children: React.ReactNode;
  bottom: number;
  fontSize: number;
}> = ({ children, bottom, fontSize }) => (
  <div
    style={{
      position: "absolute",
      left: "50%",
      bottom,
      transform: "translateX(-50%)",
      padding: "12px 24px",
      color: "#fff7e8",
      backgroundColor: "rgba(8, 12, 18, 0.86)",
      border: "1px solid rgba(255, 171, 55, 0.7)",
      borderRadius: 8,
      fontFamily: "Segoe UI, Meiryo, sans-serif",
      fontSize,
      fontWeight: 700,
      letterSpacing: "0.02em",
      whiteSpace: "nowrap",
      boxShadow: "0 4px 18px rgba(0, 0, 0, 0.38)",
    }}
  >
    {children}
  </div>
);

const BrandBackground: React.FC<{ children: React.ReactNode }> = ({ children }) => (
  <AbsoluteFill
    style={{
      overflow: "hidden",
      background: "radial-gradient(circle at 50% 43%, #202a38 0%, #10151d 38%, #07090d 76%)",
      color: "#fff7e8",
      fontFamily: "Segoe UI, Meiryo, sans-serif",
      alignItems: "center",
      justifyContent: "center",
    }}
  >
    {[320, 500, 700, 920].map((size) => (
      <div
        key={size}
        style={{
          position: "absolute",
          width: size,
          height: size,
          borderRadius: "50%",
          border: "1px solid rgba(255, 171, 55, 0.10)",
        }}
      />
    ))}
    <div
      style={{
        position: "absolute",
        width: 18,
        height: 18,
        borderRadius: "50%",
        backgroundColor: "#ffad4a",
        boxShadow: "0 0 55px 24px rgba(255, 150, 45, 0.28)",
      }}
    />
    {children}
  </AbsoluteFill>
);

const IntroCard: React.FC = () => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();
  const titleOpacity = interpolate(frame, [0, 0.5 * fps], [0, 1], { extrapolateRight: "clamp" });
  const detailOpacity = interpolate(frame, [3.3 * fps, 4.1 * fps], [0, 1], {
    extrapolateLeft: "clamp",
    extrapolateRight: "clamp",
  });

  return (
    <BrandBackground>
      <div style={{ zIndex: 1, width: 1160, textAlign: "center", opacity: titleOpacity }}>
        <div
          style={{
            color: "#ffb45c",
            fontSize: 72,
            fontWeight: 800,
            letterSpacing: "0.09em",
            textShadow: "0 0 30px rgba(255, 161, 65, 0.25)",
          }}
        >
          NEKOSPACE BINAURAL
        </div>
        <div style={{ marginTop: 18, fontSize: 40, fontWeight: 650 }}>声を、耳元の空間へ。</div>
        <div style={{ marginTop: 48, opacity: detailOpacity, fontSize: 29, lineHeight: 1.75 }}>
          <div>モノラルの声や効果音を、ヘッドフォン向け立体音響へ</div>
          <div style={{ color: "#ffc37d", fontWeight: 700 }}>方位 ・ 高さ ・ 距離を直感的に操作</div>
        </div>
        <div
          style={{
            marginTop: 42,
            color: "rgba(255, 247, 232, 0.62)",
            fontSize: 20,
            fontWeight: 650,
            letterSpacing: "0.22em",
          }}
        >
          HEADPHONES RECOMMENDED
        </div>
      </div>
    </BrandBackground>
  );
};

const OutroCard: React.FC = () => {
  const frame = useCurrentFrame();
  const { fps } = useVideoConfig();
  const opacity = interpolate(frame, [0, 0.45 * fps], [0, 1], { extrapolateRight: "clamp" });

  return (
    <BrandBackground>
      <div style={{ zIndex: 1, textAlign: "center", opacity }}>
        <div style={{ color: "#ffb45c", fontSize: 58, fontWeight: 800, letterSpacing: "0.08em" }}>
          NEKOSPACE BINAURAL
        </div>
        <div style={{ marginTop: 24, fontSize: 29, fontWeight: 650 }}>
          Voice&nbsp;&nbsp;/&nbsp;&nbsp;ASMR&nbsp;&nbsp;/&nbsp;&nbsp;Audio Drama&nbsp;&nbsp;/&nbsp;&nbsp;Sound Effects
        </div>
        <div
          style={{
            marginTop: 38,
            color: "rgba(255, 247, 232, 0.66)",
            fontSize: 20,
            letterSpacing: "0.18em",
          }}
        >
          HEADPHONES RECOMMENDED
        </div>
        <div style={{ marginTop: 68, color: "rgba(255, 247, 232, 0.72)", fontSize: 20 }}>
          音声：VOICEVOX:猫使アル
        </div>
      </div>
    </BrandBackground>
  );
};

export const NekoSpaceBinauralDemo: React.FC = () => {
  const { fps } = useVideoConfig();

  return (
    <AbsoluteFill style={{ backgroundColor: "#0b0d12" }}>
      <Sequence durationInFrames={DEMO_START_FRAME}>
        <IntroCard />
      </Sequence>

      <Sequence from={DEMO_START_FRAME} durationInFrames={DEMO_DURATION_FRAMES}>
        <Video
          src={staticFile("NekoSpace_Binaural_demo_cut.mp4")}
          style={{ width: "100%", height: "100%" }}
          objectFit="contain"
          volume={(mediaFrame) => sourceVolumeAtFrame(mediaFrame, fps)}
        />
      </Sequence>

      {NARRATION.map((narration) => (
        <Sequence
          key={narration.file}
          from={Math.round(narration.fromSeconds * fps)}
          durationInFrames={Math.round(narration.durationSeconds * fps)}
        >
          <Audio src={staticFile(narration.file)} volume={0.9} />
        </Sequence>
      ))}

      {CAPTIONS.map((caption) => (
        <Sequence
          key={caption.text}
          from={DEMO_START_FRAME + Math.round(caption.fromSourceSeconds * fps)}
          durationInFrames={Math.round((caption.toSourceSeconds - caption.fromSourceSeconds) * fps)}
        >
          <OverlayText bottom={58} fontSize={30}>{caption.text}</OverlayText>
        </Sequence>
      ))}

      <Sequence from={OUTRO_START_FRAME} durationInFrames={Math.round(OUTRO_SECONDS * fps)}>
        <OutroCard />
      </Sequence>
    </AbsoluteFill>
  );
};
