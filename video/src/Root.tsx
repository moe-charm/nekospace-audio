import "./index.css";
import { Composition } from "remotion";
import {
  NekoSpaceBinauralDemo,
  VIDEO_DURATION_FRAMES,
  VIDEO_FPS,
  VIDEO_HEIGHT,
  VIDEO_WIDTH,
} from "./Composition";

export const RemotionRoot: React.FC = () => {
  return (
    <Composition
      id="NekoSpaceBinauralDemo"
      component={NekoSpaceBinauralDemo}
      durationInFrames={VIDEO_DURATION_FRAMES}
      fps={VIDEO_FPS}
      width={VIDEO_WIDTH}
      height={VIDEO_HEIGHT}
    />
  );
};
