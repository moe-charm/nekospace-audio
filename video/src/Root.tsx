import "./index.css";
import { Composition } from "remotion";
import {
  NekoSpaceBinauralDemo,
  VIDEO_DURATION_FRAMES,
  VIDEO_FPS,
  VIDEO_HEIGHT,
  VIDEO_WIDTH,
} from "./Composition";
import {
  NekoSpaceReverbDemo,
  REVERB_DURATION_FRAMES,
  REVERB_FPS,
  REVERB_HEIGHT,
  REVERB_WIDTH,
} from "./ReverbComposition";

export const RemotionRoot: React.FC = () => {
  return (
    <>
      <Composition
        id="NekoSpaceBinauralDemo"
        component={NekoSpaceBinauralDemo}
        durationInFrames={VIDEO_DURATION_FRAMES}
        fps={VIDEO_FPS}
        width={VIDEO_WIDTH}
        height={VIDEO_HEIGHT}
      />
      <Composition
        id="NekoSpaceReverbDemo"
        component={NekoSpaceReverbDemo}
        durationInFrames={REVERB_DURATION_FRAMES}
        fps={REVERB_FPS}
        width={REVERB_WIDTH}
        height={REVERB_HEIGHT}
      />
    </>
  );
};
