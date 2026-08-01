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
      <CHOICE id="source.mode"  name="Mono Object"/>
      <CHOICE id="quality.mode" name="Standard"/>
      <CHOICE id="hrtf.profile" name="Analytic B"/>
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
2. **New parameters are appended**, never inserted, and take an **incremented version
   hint** in `ParameterID { id, versionHint }` — that is what the hint is for.
3. **Choice parameters are stored by name, never by index or normalised value.**
   A choice normalises as `index / (count - 1)`, so adding one option rewrites what every
   existing project means: "Analytic B" saved as 0.5 of three options decodes to index 2
   of four — a different profile. `hrtf.profile` has already gone from two options to
   four, so this is a bug that was live, not a precaution. On load the name wins over the
   normalised value; an unrecognised name falls back to whatever the parameter already
   holds.
4. **Adding a choice option is safe. Renaming one is not** — it silently becomes
   "unknown" for older projects. Rename only with a migration.
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
| 1 | Original. Elevation values were loose properties on the root; choice parameters relied on the normalised value, so growing a choice list reinterpreted old projects. |
| 2 | `NEKOSPACE_EXTRA` child with its own version; choice parameters stored by name. Schema 1 states still load: the loose elevation properties are read, and choices fall back to the normalised value. |

## Known semantic change, pre-release

`nearfield.amount` was re-specified after the value range was already in use: 0 % used to
mean "per-ear distance via the Woodworth approximation" and now means "both ears at the
same distance". A project saved before that change will sound different. This was not
migrated because nothing had been released — **after the first release, a change like
this requires a schema bump and a migration step**, per rule 8.
