# shared/

Contains the product-neutral, JUCE-free DSP primitives that now have at least two real
consumers.

Code moves here when a **second** plugin actually needs it — not when it merely looks
reusable. See [docs/repo-layout.md](../docs/repo-layout.md).

The first promoted primitive is `nekospace/dsp/FractionalDelay.h`. Binaural keeps an `nsb` alias so
the extraction does not alter its source-level API or arithmetic. Reverb consumes the
product-neutral type directly.

When the first thing is promoted, add a `CMakeLists.txt` here defining a product-neutral
target such as `nekospace_dsp`; the top-level build picks it up automatically. Do not use
the Binaural-specific `nsb` prefix for an API shared by the suite.
