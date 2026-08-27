# NekoSpace Binaural demo video

Remotion composition for the NekoSpace Binaural Player demonstration.

The private recording is copied to `public/NekoSpace_Binaural_demo_cut.mp4` for
local preview and rendering. The repository-wide `*.mp4` rule keeps that asset
out of Git; only the composition and caption timing are versioned.

## Commands

Install dependencies:

```console
npm install
```

Preview in Remotion Studio:

```console
npm run dev
```

Render the titled, narrated and captioned MP4:

```console
npx remotion render NekoSpaceBinauralDemo out/NekoSpace_Binaural_demo_final.mp4
```

Adjust the event ranges in `src/Composition.tsx` when a new recording is made.

The Reverb preset demonstration uses composition `NekoSpaceReverbDemo`, source timing in
`src/ReverbComposition.tsx`, and the ignored local asset
`public/NekoSpace_Reverb_demo_cut.mp4`.

Generate and render it with:

```console
npm run narration:reverb
npm run lint
npx remotion render NekoSpaceReverbDemo out/NekoSpace_Reverb_demo_final.mp4
```

Generate the seven narration clips while the local VOICEVOX engine is running:

```console
npm run narration
```

The selected voice is `VOICEVOX:猫使アル` (`おちつき`, speaker ID 56). The
required credit is burned into the end of the composition. See
`../docs/video-production.md` for the script, asset boundary and publication
checklist.

## License

The composition source is licensed under AGPL-3.0-or-later with the rest of this
repository. Private recordings, generated narration and rendered videos stay ignored.
Remotion's own license terms apply to the Remotion packages; see
<https://www.remotion.dev/>.
