# NekoSpace Reverb state format

The first public format is APVTS state root `NekoSpaceReverbState` with
`schemaVersion = 1`. Every host parameter in [parameter-contract.md](parameter-contract.md)
is stored by its permanent string ID.

```xml
<NekoSpaceReverbState schemaVersion="1">
  <PARAM id="reverb.bypass" value="0"/>
  ...
  <PARAM id="reverb.wetMonoInput" value="0"/>
</NekoSpaceReverbState>
```

## Schema 1 rules

- Unknown properties and children are ignored.
- A missing parameter takes that parameter's documented default.
- Unsaved audition mode is deliberately absent; loading state does not change it.
- Factory preset names are absent; the exact parameter tuple is the state.
- Every successful load is normalized to the current root and schema before the next save.
- Meaning changes require a schema bump and explicit migration. Adding a parameter alone
  does not require a bump when its missing-value default is correct for old projects.

## Development-state bridge

Before release, builds wrote root `NekoSpaceReverbPrototypeState` with schema 0. Those
files were never a public contract, but accepting them avoids destroying the owner's
existing FL Studio experiments. The schema-1 reader accepts that exact legacy root,
restores known IDs, supplies defaults for missing newer IDs, and emits only the formal
schema-1 root on the next save.

Any other root is rejected rather than guessed. A future schema reader first checks the
version, migrates known older meaning, preserves unknown additions where feasible, then
publishes one complete state to APVTS.

