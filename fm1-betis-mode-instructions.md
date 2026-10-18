# FM1 Betis Mode: firmware implementation

The complete hardware and web application is in `vendor/fm1_open/`, pinned to
M-VAVE's official FM1 Open V13 commit
`b178ac43c25f010c60a878056fb5331fe693fd62`.
See [README.md](README.md) for controls, downloads, build, verification and
installation precautions.

## Authoritative hardware path

- PI32v2 clang 4.0.1 r3, specifically JieLi toolchain installer 2.5.2.
- Real board drivers, 240x240 RGB565 TFT, 44.1 kHz native engine.
- Official SDK manifest, toolchain hashes, linker and SRAM checks are retained.
- Official source packager generates a real `.fwsc`, not a renamed ELF.
- The SDK and boot/update/recovery resources keep their original licenses.

`tools/Get-Toolchains.ps1` downloads and validates the exact compiler releases.
`Build.ps1` calls the official package build, generates the corresponding-source
archive and stages the complete workbench in `docs/`.
`tools/Verify.ps1` checks layout, runtime/ABI reports, checksums, embedded
payload identity and corresponding-source completeness.

## Betis additions

- `app/betis.c`: E Phrygian-dominant cent tuning using the official tuning
  API, track 1 arpeggiator defaults, and synthesized 808 crowd rhythm.
- `app/betis.h`: shared hardware/web interface and dashboard page ABI.
- `app/betis_pixels.h`: original 32x40 indexed-color shield, mascot and eleven
  player portraits, compressed losslessly; `tools/Generate-Betis-Art.py` reproduces
  both the native artwork and `web/betis-sprites.svg` contact sheet.
- `app/screen.c`: green theme, boot branding, dashboard and controls, animated
  MIDI activity, and HOME+FX navigation. Uses the original band compositor,
  DMA ownership and existing input/transport handlers.
- FX cycles studio / La Grada / representative 2025/26 XI / club history /
  Himno-Aprender. La Grada offers
  four original percussion patterns (EDIT explicitly loads track 8), a manual
  0-99 scoreboard and a four-second goal celebration. The XI is explicitly a
  project selection from the 2025/26 squad, not an official fixed match lineup.
- `tools/Compile-Hymn.py` extracts a monophonic top-line melody from a selected
  track in user-supplied MIDI, retaining tempo and folding octaves to 27 keys.
  The ignored generated `app/betis_song_data.h` is embedded only in a private
  `Build.ps1 -LocalHymn` build, distributed in ignored `local/docs/`.
  Public builds refuse to run if that header is present. See README for the
  matching private source archive and public source-URL caveat.
- Aprender: EDIT starts, the next physical/on-screen key lights, correct new
  key presses advance one step, errors wait, K1 seeks and REC repeats a group
  of eight notes. SCL stops; HOME exits and restores tuning/ARP. Input evaluation
  uses the panel/on-screen keyboard, not external MIDI. PLAY listens to the
  embedded melody. Projects and samples are not written by the lesson.
- `web/betis-grada.html`: web-only local authorized-audio players for traditional
  and centenary hymns. No protected music or lyrics are supplied; selected files
  are neither uploaded nor persisted. Native patterns remain original percussion.
- `web/betis-midi.js`: local MIDI-to-FM adaptation, tempo/sustain preservation,
  eight-voice limit, type-0 MIDI export and real synth playback. Supplied
  originals and derivatives in ignored `himnos/` are not redistributed.
- `web/engine/Simulator.c`: starts the same dashboard in the Wasm platform.

The native six-operator FM and other official engines are retained instead
of using the earlier standalone two-operator preview. Pasión uses official
FM feedback, Olé uses reverb send, and Duende uses delay send. There is no
new native saturation/chorus DSP. Quantization preserves MIDI note identities
and does not affect percussion. Project loading restores project tuning.

The browser is built with WASI SDK 27 from the portable upstream app/engine
sources, using the same UI and tuning APIs as the native application.
`tests/verify-emulator.html` boots that Wasm for behavioral acceptance tests.
The root `src/` and `web/` directories are an undeployed legacy prototype.

## Safety and distribution

No device flashing, persistent device writes, or USB/BLE connections occur
as part of a build or test. Actual flashing requires an explicit user action
in the workbench. Back up projects/samples and keep power stable.

The static flash layout, runtime symbols and Wasm behavior can be verified
without a device. Electrical timing, radio behavior, physical controls,
audio deadlines and recovery on real hardware remain untested. Do not mark
`hardware_validated` true without physical test evidence.

The generated workbench and source archive are served together by GitHub
Pages. Preserve license notices and distribute the matching source archive
with every updated binary. Do not change SDK hashes or skip its validations
to force an incompatible toolchain or firmware image through.
