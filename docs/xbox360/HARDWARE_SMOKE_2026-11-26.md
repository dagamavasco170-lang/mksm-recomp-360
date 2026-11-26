# Xbox 360 physical smoke — 2026-11-26

## Scope

This is a diagnostic XEX validation on a physical Xbox 360 RGH/JTAG. It is **not a playable game build** and does not demonstrate retail-game boot.

## Verified diagnostic markers

- Console-interactive XEX entry.
- 32-bit big-endian PowerPC ABI.
- EE trace signature: `4F63CD2AAFC519CE`, failures `00000000`, PC `00000048`, retired instructions `17`.
- LQ/SQ and unaligned merge probes: failures `00000000`.
- Unaligned merge probe exercised four byte offsets.
- SPR/DMA trace: failures `00000000`, 32 bytes, 768 causes.
- Generated Xenon, LQ/SQ, unaligned merge, and SPR/DMA traces: all failures `00000000`.
- C++20, guest-memory endian, and scalar packed-vector checks passed.
- 1280x720 display opened and line counter reached 720.
- Xbox Back mapped to PS2 Select.
- 376 frames presented; Back exited; process status 0.

## Remaining file-I/O gate

The console log reported:

`MKSM_IOMAN_BRIDGE status=not-configured hint=cmake-MKSM_DISC_ROOT`

Next test is a read-only probe against an extracted retail asset, `GAMEDATA.WAD`: open, read 16 bytes, seek, reread, compare, and close. Game assets must remain local and must not be committed or uploaded.

## CI artifacts

- Console smoke XEX: [run 45810933859 artifact 15114170673](https://github.com/dagamavasco170-lang/mksm-recomp-360/actions/runs/45810933859/artifacts/15114170673)
- Xenia headless XEX: [artifact 15114290526](https://github.com/dagamavasco170-lang/mksm-recomp-360/actions/runs/45810933859/artifacts/15114290526)
- Xenia logs: [artifact 15113820731](https://github.com/dagamavasco170-lang/mksm-recomp-360/actions/runs/45810933859/artifacts/15113820731)

## Status

Hardware smoke gate is closed for this diagnostic revision only. Gameplay boot, retail IOMAN integration, IOP execution, GS-to-Xenos rendering, VU, audio, FMV, save support, and full game runtime remain incomplete or unverified.
