# Repository README Structure

This document is the source of truth for the top-level English and Japanese README files.
The repository is a product suite, so the landing page must not look like a single Binaural
plug-in with unrelated experiments attached.

## Information order

The top-level README follows this order:

1. suite name, one-sentence purpose and language link;
2. compact product/status table;
3. one equal-weight visual card for each implemented product;
4. published and local-demo status;
5. short honest status and limitations for each product;
6. installation, build and repository layout;
7. shared contracts and licence.

Detailed operation manuals, research history and long limitation explanations belong in each
product's README. The landing page links to them instead of duplicating them.

## Product cards

The implemented products are presented in this order:

| Product | Landing-page promise | Image |
| --- | --- | --- |
| NekoSpace Binaural | place voice and effects around the listener | `docs/images/gui-main.png` |
| NekoSpace Reverb | build a natural room around an existing stereo image | `docs/images/reverb-main.png` |
| NekoSpace CleanVoice | learn a noise-only region, compare Original/Clean/Removed | `docs/images/cleanvoice-main.png` |

Images show only application chrome and generated/default UI state. They must not reveal a
private take name, waveform, local path, project name or other production material. Product
cards use the same heading depth, status vocabulary and approximate image width.

## Demo boundary

- Published YouTube videos may be linked from the top-level README.
- A locally rendered but unpublished video is described as `local demo ready`; it is never
  linked with a filesystem path.
- Recordings, generated narration and rendered media remain ignored globally.
- The regular loop source is a feature-tour aid. A later sound-quality demo uses separately
  cleared classical-style and voice sources.

## Status vocabulary

- **Alpha**: packaged public build with its declared release gates complete.
- **Owner-audition prototype**: real application and DSP accepted for the current direction,
  but public packaging or validation gates remain.
- **Prototype**: usable development application whose product contract is not frozen.
- **Planned**: documentation or intent only; no product card image is required.

The README must not collapse engineering tests, owner listening, host smoke tests and public
release readiness into one claim.
