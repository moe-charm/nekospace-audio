# Repository README Structure

This document is the source of truth for the top-level English and Japanese README files.
The repository is a product suite, but its landing page must distinguish the packaged public
product from source-built development products. It must not imply equal release readiness.

## Information order

The top-level README follows this order:

1. publication-safe `docs/images/banner.jpg`, suite name, badges and one-sentence purpose;
2. direct download CTA for the current packaged release;
3. the CleanVoice -> Binaural -> Reverb production path;
4. a full-width hero for the current public product;
5. secondary cards for source-built development products;
6. compact honest status and limitations;
7. installation, build, repository layout, shared contracts and licence.

Detailed operation manuals, research history and long limitation explanations belong in each
product's README. The landing page links to them instead of duplicating them.

## Product hierarchy

The products form one workflow, but presentation follows release readiness:

| Product | Landing-page role | Image |
| --- | --- | --- |
| NekoSpace Binaural | full-width featured product with direct release CTA | `docs/images/gui-main.png` |
| NekoSpace Reverb | half-width development card | `docs/images/reverb-main.png` |
| NekoSpace CleanVoice | half-width development card | `docs/images/cleanvoice-main.png` |

Images show only application chrome and generated/default UI state. They must not reveal a
private take name, waveform, local path, project name or other production material. Product
The banner is decorative and contains no generated lettering; the Markdown heading remains
the canonical brand text. Development cards share heading depth, vocabulary and image width.

## Demo boundary

- Published YouTube videos may be linked from the top-level README.
- A locally rendered but unpublished video is described as `local demo ready`; it is never
  linked with a filesystem path.
- Recordings, generated narration and rendered media remain ignored globally.
- The regular loop source is a feature-tour aid. A later sound-quality demo uses separately
  cleared classical-style and voice sources.

## Status vocabulary

- **Alpha**: packaged public build with its declared release gates complete.
- **In development — DSP and UI working**: real application and DSP accepted for the current
  direction, but public packaging or validation gates remain.
- **Prototype — offline app and CLI**: usable development application whose product contract
  is not frozen.
- **Planned**: documentation or intent only; no product card image is required.

The README must not collapse engineering tests, owner listening, host smoke tests and public
release readiness into one claim.
