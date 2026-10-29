# Watchsync enclosure

Original design by axelaberg91, CC BY-NC 4.0. See the root licensing notice.

`watchsync-enclosure.scad` is the current editable enclosure model. The bottom
plate is revision v14; the upper shell and coil former retain their existing
geometry. Select a part using the model's `part` parameter. Finished STL files
and their checksums are distributed as the versioned `hardware-v14` release
package, rather than duplicated in the source tree.

The release must contain `watchsync-base.stl`, `watchsync-lid.stl` and
`watchsync-bobbin.stl`. Preserve the published files and checksums when preparing
the release. Changing the repository layout does not validate a physical print.

## Underside markings

All three underside text rows are uppercase and use the same bold font size:

- WATCHSYNC
- TIME SIGNAL TRANSMITTER
- WATCHSYNC.AXELABERG91.WORKERS.DEV

The uppercase address is fitted to 102 mm width and retains the raised position
from v13. Row centres remain 13.3 mm apart. The QR target and 50.9 mm footprint
are unchanged. Mounting points and all other mechanical geometry are retained.

Print `watchsync-base.stl` underside down using PLA and 0.20 mm first and
subsequent layers. For the recessed markings, use opaque light PLA at Z=0.20
and 0.40 mm, then change to opaque dark PLA before Z=0.60 mm. Recess depth is
0.4 mm. Reuse the existing upper shell and coil former.

The reference printer is a Prusa CORE One with PLA. `verification.json` records
the existing digital v14 bottom-plate checks. It does not establish physical
fit, strength, printability on every printer, electrical safety or device
operation; its `physicalPrintVerified` value remains false.
