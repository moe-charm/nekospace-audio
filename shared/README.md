# shared/

Empty on purpose.

Code moves here when a **second** plugin actually needs it — not when it merely looks
reusable. See [docs/repo-layout.md](../docs/repo-layout.md).

Likely first candidates, currently living in `plugins/binaural/src/dsp/`:
`FractionalDelay`, `CrossfadeFir`, `FdnReverb`, the smoothers and biquad helpers.

When the first thing is promoted, add a `CMakeLists.txt` here defining a `nsb_shared`
interface library; the top-level build picks it up automatically.
