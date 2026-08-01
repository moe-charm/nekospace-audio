# State Format

What the plugin writes into a host project, and the rules that keep old projects loading
correctly as the plugin grows. FL Studio automation binds to VST3 parameter IDs, so the
parameter contract and this document together define compatibility.

## Layout (schema 2)

```xml
<NekoSpaceState schemaVersion="2" uiWidth="1000" uiHeight="640">
  <PARAM id="position.azimuth" value="0.5"/>
  ...
  <NEKOSPACE_EXTRA version="1">
    <CHOICES>
      <CHOICE id="source.mode"  key="mono_object" name="Mono Object"/>
      <CHOICE id="quality.mode" key="standard" name="Standard"/>
      <CHOICE id="hrtf.profile" key="analytic_b" name="Analytic B"/>
    </CHOICES>
    <ELEVATION>
      <ANCHOR which="below" notchHz="4967" notchDb="-8.4" .../>
      <ANCHOR which="level" .../>
      <ANCHOR which="above" .../>
      <MACROS up="1" down="1" body="1" focus="1"/>
    </ELEVATION>
  </NEKOSPACE_EXTRA>
</NekoSpaceState>
```

The root is the APVTS tree, so JUCE's own parameter restore works untouched. Everything
that is not an automatable parameter lives in `NEKOSPACE_EXTRA`, which carries its own
version independent of the schema version.

## Rules

1. **Parameter IDs are permanent.** Never rename, never reuse. Retiring one means it
   stops being written; a state that still contains it is ignored, not rejected.
2. **New parameters use a higher `versionHint` than every earlier release.** The hint
   preserves Audio Unit ordering in Logic/GarageBand; appending also keeps host display
   order predictable, but the hint is not a state-schema version.
3. **Choice values get permanent machine keys.** `name` is only a readable snapshot and a
   compatibility bridge for early schema-2 development states. Keys such as `analytic_b`
   are never renamed, translated, reordered, or reused. APVTS stores the denormalised
   choice index in each root `PARAM`; the key wins over that index on state load, and an
   unknown key retains the pre-restore parameter value.
4. **Choice lists are frozen after release.** Stable keys protect plugin state, but hosts
   store automation as `index / (count - 1)`, which keys cannot repair. Adding, removing,
   or reordering an option after release requires a new parameter ID. Display names may
   change freely because they are not identifiers.
5. **Unknown properties and children are ignored, never rejected.** A project written by
   a newer build must still open in an older one, minus the features it does not have.
6. **Missing fields keep their defaults.** Every reader supplies a fallback, so adding a
   field later cannot invalidate an older project.
7. **`schemaVersion` is bumped only when the meaning of stored data changes**, not when
   data is added. Additions are handled by rules 5 and 6.
8. **Semantic changes to an existing parameter need a version bump and a migration.**
   Changing what a value *means* is the one thing rules 5 and 6 cannot absorb.

## Version history

| Schema | Change |
| --- | --- |
| 1 | Original. Elevation values were loose properties on the root; APVTS choice values were stored as raw indices. |
| 2 | `NEKOSPACE_EXTRA` child with its own version; choice values also stored by stable key. Early schema-2 development states that contain only `name` still load. Schema 1 raw choice indices and loose elevation properties remain readable. |

### APVTS state versus host automation

These are deliberately treated as separate formats. JUCE APVTS serialises the
**denormalised** choice index (`1` for the second item), so appending an item does not turn
an old index 1 into another choice. VST3/AU automation is host-owned and uses a normalised
0..1 value, so changing the number or order of choices can reinterpret existing automation.
The stable-key layer protects project/preset state; rule 4 protects automation.

`NEKOSPACE_EXTRA.version` is read independently from `schemaVersion`. Readers parse the
fields they understand from version 1 and later and ignore unknown children, preserving
forward compatibility.

## Known semantic change, pre-release

`nearfield.amount` was re-specified after the value range was already in use: 0 % used to
mean "per-ear distance via the Woodworth approximation" and now means "both ears at the
same distance". A project saved before that change will sound different. This was not
migrated because nothing had been released — **after the first release, a change like
this requires a schema bump and a migration step**, per rule 8.
