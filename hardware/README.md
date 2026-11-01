# Watchsync enclosure

Original design by axelaberg91, CC BY-NC 4.0. See the root licensing notice.

The QR generator uses [python-qrcode 8.2](https://github.com/lincolnloop/python-qrcode/tree/v8.2),
which retains its [BSD/MIT licensing notices](https://github.com/lincolnloop/python-qrcode/blob/v8.2/LICENSE).

`watchsync-enclosure.scad` is the current editable enclosure model. The bottom
plate is revision v15; the upper shell and coil former retain their existing
geometry. Select a part using the model's `part` parameter. Finished STL files
and their checksums are distributed as the versioned `hardware-v15` release
package, rather than duplicated in the source tree.

The release must contain `watchsync-base.stl`, `watchsync-lid.stl` and
`watchsync-bobbin.stl`. Preserve the published files and checksums when preparing
the release. Changing the repository layout does not validate a physical print.

## Underside markings

All three underside text rows use the same bold font size:

- WATCHSYNC
- TIME SIGNAL TRANSMITTER
- axelaberg91.github.io/watchsync/

The address uses the exact lowercase path and is centred within the existing
102 mm marking area. Row centres remain 13.3 mm apart. Both the address and QR
code point to `https://axelaberg91.github.io/watchsync/`. The QR footprint remains
50.9 mm wide, including four quiet modules on each side. Mounting points and all
other mechanical geometry are retained from v14.

To reproduce the QR definition, install `qrcode==8.2`, then run from the repository
root:

```sh
python tools/generate_hardware_qr.py --url https://axelaberg91.github.io/watchsync/ --check
```

Omit `--check` to regenerate the QR cells after an intentional destination change.
The generator preserves the physical width and updates the matrix dimensions;
it does not change the printed address. Review both together before a new release.

Print `watchsync-base.stl` underside down using PLA and 0.20 mm first and
subsequent layers. For the recessed markings, use opaque light PLA at Z=0.20
and 0.40 mm, then change to opaque dark PLA before Z=0.60 mm. Recess depth is
0.4 mm. Reuse the existing upper shell and coil former.

The reference printer is a Prusa CORE One with PLA. `verification.json` records
the design revision, destination and nominal marking dimensions. It is not a
physical test report. Physical fit, strength, printability on every printer,
electrical safety and device operation are not established by the design;
`physicalPrintVerified` remains false.
