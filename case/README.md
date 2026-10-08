# Case

The housing is built by a Python script, not drawn in a CAD program.

## Print these (`stl/`)

| File | Count | Notes |
|---|---|---|
| `tv_body_v8_no_sensor_hole.stl` | 1 | The body. Print front face down, as saved. |
| `tv_body_v8.stl` | alternative | Same body with a small hole over the board's light sensor. This firmware does not use the sensor, so only pick this one if you plan to add that yourself. |
| `tv_stand_v8.stl` | 1 | |
| `tv_knob_magnetic_x2.stl` | 2 | Takes a 5 x 2 mm magnet. |
| `tv_antenna_base.stl` | 1 | |
| `tv_antenna_tip_x2.stl` | 2 | Rods are 1.75 mm filament offcuts. |
| `tv_rear_cap_v8.stl` | optional | Closes the back. |
| `FIT_TEST_usb_and_sd_end.stl` | optional | Small piece to check USB-C and SD card fit before printing the whole body. |
| `FIT_TEST_knob_socket.stl` | optional | Small piece to check magnet and knob fit. |

The board is held in the body with M3 brass threaded inserts and two M3 x 6 mm hex screws (an M3 nut on each screw as a spacer).

## Rebuild the STL files

1. Download the original CYD front case (link in `LICENSE.md`) and save it as `original/2.8inch_Front_Case.stl`.
2. `pip install trimesh manifold3d numpy`
3. From this folder: `python build_tv.py`

The script writes the body, rear cap, stand, knob and both test pieces into `stl/`. Every dimension is a named number near the top. The antenna parts and the no-hole body come from earlier one-off scripts and are included as STL only.
