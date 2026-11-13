# Watchsync enclosure

Original design by axelaberg91, CC BY-NC 4.0. See the root licensing notice.

The QR generator uses [python-qrcode 8.2](https://github.com/lincolnloop/python-qrcode/tree/v8.2),
which retains its [BSD/MIT licensing notices](https://github.com/lincolnloop/python-qrcode/blob/v8.2/LICENSE).

`watchsync-enclosure.scad` is the current editable enclosure model. The bottom
plate is the unreleased revision v16 prototype; the upper shell and coil former retain their existing
geometry. Select a part using the model's `part` parameter. The current printable
parts are also available directly in this directory:

- [Bottom plate](watchsync-base.stl)
- [Upper shell](watchsync-lid.stl)
- [Coil former](watchsync-bobbin.stl)

The bottom plate is the v16 review prototype. The upper shell and coil former
remain byte-for-byte copies of
[hardware-v15](https://github.com/axelaberg91/watchsync/releases/tag/hardware-v15).
That release remains the published download until v16 is approved and released.
Older revisions and combined ZIP packages remain available through Releases.

The release must contain `watchsync-base.stl`, `watchsync-lid.stl` and
`watchsync-bobbin.stl`. Preserve the published files and checksums when preparing
the release. Changing the repository layout does not validate a physical print.

## Underside markings

The printed website address is removed. The remaining two centred rows are:

- WATCHSYNC: approximately 45.4 mm wide.
- TIME SIGNAL TRANSMITTER: 100 mm wide, leaving 3.2 mm per side on the
  106.4 mm plate (5 mm to the assembled case's outer edges).

Both use approximately 5.5 mm bold text, with row centres 20 mm apart. The QR
code still points to `https://axelaberg91.github.io/watchsync/`. Its visible
symbol grows from approximately 39.9 mm to 48.9 mm, about 4.5 mm per side.
Mounting points and mechanical geometry are unchanged.

The enlarged symbol leaves approximately 1.05 mm to the plate edge and 1.15 mm
to the centre screw recess. The QR matrix contains four nominal border modules,
but that complete quiet zone does not fit on the physical plate. A successful
digital decode does not establish reliable phone scanning on a print. Treat v16
as a prototype until the actual printed QR code has been tested with the screw
installed and the intended filament colours.

To reproduce the QR definition, install `qrcode==8.2`, then run from the repository
root:

```sh
python tools/generate_hardware_qr.py --url https://axelaberg91.github.io/watchsync/ --check
```

Omit `--check` to regenerate the QR cells after an intentional destination change.
The generator preserves the physical width and updates the matrix dimensions;
it does not validate the physical quiet zone or alter the lettering. Review the
complete layout before a new release.

Print `watchsync-base.stl` underside down using PLA and 0.20 mm first and
subsequent layers. For the recessed markings, use opaque light PLA at Z=0.20
and 0.40 mm, then change to opaque dark PLA before Z=0.60 mm. Recess depth is
0.4 mm. Reuse the existing upper shell and coil former.

The reference printer is a Prusa CORE One with PLA. `verification.json` records
the design revision, destination and nominal marking dimensions. It is not a
physical test report. Physical fit, strength, printability on every printer,
electrical safety and device operation are not established by the design;
`physicalPrintVerified` remains false.
