// SPDX-License-Identifier: CC-BY-NC-4.0
// Original watchsync prototype CAD by axelaberg91; CC BY-NC 4.0. See LICENSE.md.
// Review prototype v14. Dimensions in mm. Positive Y is the USB/rear edge.
// Button, reset, USB and wire clearances still require a physical fit check.

part = "assembly"; // [assembly,exploded,base,lid,bobbin,atom,winding]
quality = 96;
$fn = quality;
eps = 0.04;

case_size = 110;
case_height = 15.2;
corner_r = 10;
plate_z = 0.4;
plate_thickness = 1.2;
floor_h = plate_z+plate_thickness;
wall = 1.6;
lid_h = 1.0;
roof_z = case_height - lid_h;
plate_clearance = 0.2;
plate_size = case_size-2*(wall+plate_clearance);
plate_corner_r = corner_r-wall-plate_clearance;

coil_xy = [-7, -7];
flange_d = 88;
core_d = 82;
core_wall = 1.6;
lower_flange = 1.0;
winding_h = 5.5;
upper_flange = 0.8;
flare_h = (flange_d-core_d)/2;
bobbin_h = lower_flange + winding_h + flare_h + upper_flange;

// The nominal rear face is flush with the shell exterior at Y=55 mm.
atom_xy = [34, 43];
atom_z = 4.2;
atom_size = [24, 24, 9.5072];
atom_clearance = 0.15;
// Uniform clearance around the nominal rounded Atom envelope.
atom_wall_clearance = 0.15;
atom_corner_r = 2.0;
reset_y = atom_xy[1]-5.5;
reset_z = atom_z+6.1;
reset_d = 2.4;

button_xy = atom_xy;
button_size = [12, 14];
button_r = 2.0;
button_slit = 0.6;
button_root_y = 5.2;
button_contact_z = atom_z+9.65;
button_rotation = 0;
light_slot = [4.4, 1.4];

// Bottom-view pin map reflected into the mounted top-view coordinates.
// USB points towards +Y; G25 and GND are on the +X header row.
// Nominal row offset estimated from the drawing; the pocket provides margin.
header_x_offset = 8.0;
header_pitch = 2.54;
g25_xy = atom_xy+[header_x_offset,header_pitch];
gnd_xy = atom_xy+[header_x_offset,3*header_pitch];
wire_channel_w = 6.0;
wire_channel_depth = 0.5;
connector_gap = 1.5;
connector_support_h = atom_z-connector_gap-floor_h;
connector_support_w = 6.0;
underside_engraving_depth = 0.4;
wire_bend_xy = coil_xy+[26,26];
wire_coil_exit_xy = coil_xy+[33,33];

// Recessed top graphics leave 0.7 mm of lid below the engraving.
engraving_depth = 0.3;
label_size = 6.0;
label_weight_offset = 0.1;
marker_stroke = 1.2;
marker_x_span = 20;

// Clips grip the lower shoulder of an open relief above the winding.
bobbin_inner_r = (core_d-2*core_wall)/2;
snap_angles = [90,210,330];
snap_sweep = 11.0;
snap_stem_t = 2.0;
snap_root_r = bobbin_inner_r-0.15;
snap_contact_r = bobbin_inner_r+0.05;
snap_hook_r = snap_contact_r+0.55;
snap_hook_z = 8.9;
snap_hook_h = 0.8;
snap_groove_z = 8.8;
snap_groove_depth = 0.9;

// M2 x 6 socket-head case screws; separate M2 x 4 Atom screw.
screw_xy = [[0,0], [-48,-48], [-48,48], [48,-48], [50,48]];
base_post_top = 4.0;
lid_post_bottom = base_post_top;
screw_clearance_d = 2.3;
pilot_d = 1.7;
head_recess_d = 4.2;
// Absolute head bearing plane, retained to preserve screw engagement.
head_seat_z = 2.2;

module rr(size, radius) {
    offset(r=radius) square([size-2*radius,size-2*radius], center=true);
}

module rounded_block(size, height, radius) {
    linear_extrude(height) rr(size, radius);
}

module rounded_rect(size, radius) {
    offset(r=radius) square([size[0]-2*radius,size[1]-2*radius],center=true);
}

module capsule_between(a, b, diameter, height) {
    hull() {
        translate([a[0],a[1],0]) cylinder(h=height,d=diameter);
        translate([b[0],b[1],0]) cylinder(h=height,d=diameter);
    }
}

module port_cuts() {
    // Use the expanded 3D case envelope, including its rounded rear corners.
    // The Atom's rear face fills this window; no cable tunnel or roof notch.
    intersection() {
        translate([atom_xy[0],atom_xy[1],atom_z-atom_wall_clearance])
            rounded_block(atom_size[0]+2*atom_wall_clearance,
                          atom_size[2]+2*atom_wall_clearance,
                          atom_corner_r+atom_wall_clearance);
        // Restrict this cut to the rear wall; preserve the button contact pad.
        translate([atom_xy[0]-atom_size[0]/2-1,
                   case_size/2-wall-atom_corner_r-eps,atom_z-1])
            cube([atom_size[0]+2,wall+atom_corner_r+2,atom_size[2]+2]);
    }
    // Straight tool access to the Atom's original side reset actuator.
    translate([case_size/2+1,reset_y,reset_z]) rotate([0,-90,0])
        cylinder(h=12,d=reset_d,$fn=32);
}

module resistor_saddle() {
    translate([-3,18,floor_h-eps])
        for (x=[-2.4,2.4]) translate([x,0,0]) difference() {
            translate([-0.6,-2.5,0]) cube([1.2,5,3.6+eps]);
            translate([-0.8,0,2.2]) rotate([0,90,0]) cylinder(h=1.6,d=3.6,$fn=40);
            translate([-0.8,-1.35,2.2]) cube([1.6,2.7,2]);
        }
}

module connector_support() {
    // Plain stop beneath G25/GND, with 1.5 mm nominal clearance to the Atom.
    // Height follows the mounting plane rather than a fixed added thickness.
    support_length = 2*(gnd_xy[1]-g25_xy[1]+4.4);
    // Keep the rear edge fixed and extend towards the enclosure center.
    translate([g25_xy[0]-connector_support_w/2,gnd_xy[1]+2.2-support_length,floor_h-eps])
        cube([connector_support_w,support_length,connector_support_h+eps]);
}

// QR target: https://watchsync.axelaberg91.workers.dev/
// Maximize the upright QR footprint above the centre screw in underside view.
// The code retains four quiet modules; visible engraving stays well inside
// the plate. Quiet-zone bounds: X +/-25.45, Y 2.25..53.15 mm.
qr_width = 50.9;
qr_modules = 37;
qr_pitch = qr_width/qr_modules;
qr_center = [0,27.7]; // underside view; assembled world Y is negative
qr_cells = [[4,4],[5,4],[6,4],[7,4],[8,4],[9,4],[10,4],[12,4],[13,4],[17,4],[19,4],[20,4],[22,4],[26,4],[27,4],[28,4],[29,4],[30,4],[31,4],[32,4],[4,5],[10,5],[13,5],[14,5],[16,5],[18,5],[22,5],[23,5],[24,5],[26,5],[32,5],[4,6],[6,6],[7,6],[8,6],[10,6],[14,6],[15,6],[16,6],[18,6],[20,6],[21,6],[22,6],[24,6],[26,6],[28,6],[29,6],[30,6],[32,6],[4,7],[6,7],[7,7],[8,7],[10,7],[12,7],[14,7],[16,7],[17,7],[19,7],[22,7],[23,7],[26,7],[28,7],[29,7],[30,7],[32,7],[4,8],[6,8],[7,8],[8,8],[10,8],[12,8],[14,8],[18,8],[19,8],[20,8],[22,8],[26,8],[28,8],[29,8],[30,8],[32,8],[4,9],[10,9],[12,9],[15,9],[16,9],[20,9],[21,9],[22,9],[23,9],[26,9],[32,9],[4,10],[5,10],[6,10],[7,10],[8,10],[9,10],[10,10],[12,10],[14,10],[16,10],[18,10],[20,10],[22,10],[24,10],[26,10],[27,10],[28,10],[29,10],[30,10],[31,10],[32,10],[12,11],[15,11],[16,11],[19,11],[21,11],[24,11],[4,12],[8,12],[10,12],[11,12],[12,12],[14,12],[21,12],[23,12],[24,12],[25,12],[26,12],[27,12],[28,12],[29,12],[32,12],[7,13],[8,13],[14,13],[17,13],[18,13],[19,13],[20,13],[22,13],[26,13],[27,13],[28,13],[29,13],[30,13],[31,13],[32,13],[4,14],[5,14],[6,14],[7,14],[10,14],[11,14],[13,14],[14,14],[18,14],[19,14],[21,14],[22,14],[25,14],[28,14],[32,14],[4,15],[5,15],[8,15],[9,15],[12,15],[13,15],[14,15],[15,15],[20,15],[21,15],[22,15],[25,15],[26,15],[27,15],[29,15],[31,15],[32,15],[5,16],[6,16],[8,16],[9,16],[10,16],[12,16],[13,16],[15,16],[19,16],[21,16],[25,16],[31,16],[6,17],[9,17],[11,17],[12,17],[13,17],[16,17],[17,17],[18,17],[19,17],[20,17],[25,17],[26,17],[27,17],[28,17],[29,17],[30,17],[31,17],[32,17],[4,18],[5,18],[6,18],[7,18],[8,18],[10,18],[13,18],[16,18],[18,18],[19,18],[20,18],[24,18],[25,18],[26,18],[27,18],[29,18],[30,18],[32,18],[4,19],[5,19],[6,19],[9,19],[12,19],[13,19],[18,19],[20,19],[21,19],[31,19],[32,19],[7,20],[8,20],[9,20],[10,20],[11,20],[12,20],[13,20],[15,20],[16,20],[20,20],[21,20],[23,20],[24,20],[25,20],[27,20],[31,20],[4,21],[6,21],[7,21],[9,21],[13,21],[16,21],[17,21],[19,21],[20,21],[22,21],[25,21],[26,21],[27,21],[28,21],[29,21],[31,21],[32,21],[7,22],[10,22],[13,22],[16,22],[17,22],[18,22],[19,22],[22,22],[25,22],[30,22],[32,22],[14,23],[15,23],[21,23],[22,23],[23,23],[24,23],[26,23],[27,23],[31,23],[32,23],[4,24],[5,24],[6,24],[7,24],[10,24],[11,24],[13,24],[16,24],[18,24],[19,24],[21,24],[22,24],[24,24],[25,24],[26,24],[27,24],[28,24],[29,24],[32,24],[12,25],[14,25],[15,25],[16,25],[18,25],[19,25],[20,25],[21,25],[23,25],[24,25],[28,25],[32,25],[4,26],[5,26],[6,26],[7,26],[8,26],[9,26],[10,26],[12,26],[15,26],[17,26],[18,26],[19,26],[21,26],[23,26],[24,26],[26,26],[28,26],[29,26],[30,26],[32,26],[4,27],[10,27],[15,27],[16,27],[18,27],[21,27],[22,27],[24,27],[28,27],[31,27],[4,28],[6,28],[7,28],[8,28],[10,28],[12,28],[14,28],[15,28],[16,28],[17,28],[18,28],[20,28],[21,28],[22,28],[23,28],[24,28],[25,28],[26,28],[27,28],[28,28],[29,28],[32,28],[4,29],[6,29],[7,29],[8,29],[10,29],[14,29],[15,29],[16,29],[17,29],[18,29],[19,29],[20,29],[25,29],[32,29],[4,30],[6,30],[7,30],[8,30],[10,30],[13,30],[14,30],[15,30],[18,30],[19,30],[20,30],[21,30],[25,30],[29,30],[30,30],[31,30],[32,30],[4,31],[10,31],[14,31],[15,31],[18,31],[21,31],[22,31],[24,31],[28,31],[29,31],[31,31],[32,31],[4,32],[5,32],[6,32],[7,32],[8,32],[9,32],[10,32],[12,32],[14,32],[15,32],[17,32],[18,32],[19,32],[21,32],[22,32],[24,32],[26,32],[27,32],[28,32],[31,32]];

module underside_engraving() {
    assert(underside_engraving_depth <= plate_thickness-0.8+0.000001);
    translate([0,0,plate_z+underside_engraving_depth]) rotate([180,0,0])
        linear_extrude(underside_engraving_depth+eps) {
            for (cell=qr_cells)
                translate([qr_center[0]-qr_width/2+cell[0]*qr_pitch,
                           qr_center[1]-qr_width/2+(qr_modules-1-cell[1])*qr_pitch])
                    // 0.01 mm overlap avoids corner-only CGAL connections.
                    translate([-0.005,-0.005]) square([qr_pitch+0.01,qr_pitch+0.01]);
            for (line=[
    [
        "WATCHSYNC",
        0.047419,
        -9.942209,
        3.668774
    ],
    [
        "TIME SIGNAL TRANSMITTER",
        -0.114759,
        -23.242209,
        3.668774
    ],
    [
        "WATCHSYNC.AXELABERG91.WORKERS.DEV",
        -0.052097,
        -36.541846,
        3.668774
    ]
])
                translate([line[1],line[2]])
                    text(line[0],size=line[3],font="Segoe UI:style=Bold",
                         halign="center",valign="center",spacing=1.0);
        }
}

module atom_seat() {
    // Central support uses the Atom's factory M2 mounting location.
    translate([atom_xy[0],atom_xy[1],floor_h])
        cylinder(h=atom_z-floor_h,d=6.5,$fn=48);
    atom_guides();
}

module atom_guides() {
    // Guide feet overlap the solid plate; no wire channel passes beneath them.
    guide_z = floor_h-eps;
    guide_h = atom_z+1.8-guide_z;
    translate([atom_xy[0]-12-atom_clearance-1.1,atom_xy[1]-10,guide_z]) cube([1.1,6,guide_h]);
    translate([atom_xy[0]+12+atom_clearance,atom_xy[1]+3,guide_z]) cube([1.1,4,guide_h]);
    translate([26,atom_xy[1]-12-atom_clearance-1.1,guide_z]) cube([15,1.1,guide_h]);
}

module bobbin_snap() {
    // The curved root and long arm limit bending strain in printed PLA.
    inner_root = snap_root_r-snap_stem_t;
    rotate_extrude(angle=snap_sweep,convexity=8)
        polygon(concat([
            [inner_root-1,-eps], [snap_root_r,-eps], [snap_root_r,1.2],
            [snap_contact_r,snap_hook_z], [snap_hook_r,snap_hook_z],
            [snap_hook_r,snap_hook_z+0.2],
            [snap_contact_r,snap_hook_z+snap_hook_h],
            [snap_contact_r-snap_stem_t,snap_hook_z+snap_hook_h],
            [inner_root,1.2]
        ], [for (a=[-5:-5:-90]) [inner_root-1+cos(a),1.2+sin(a)]]));
}

module wire_channels() {
    // Retain only the short crossing beneath the former's 45-degree exit.
    translate([0,0,floor_h-wire_channel_depth])
        capsule_between(wire_coil_exit_xy,wire_bend_xy,wire_channel_w,wire_channel_depth+eps);
}

module base_model() {
    difference() {
        intersection() {
            union() {
                // Flat underside prints directly on the bed after Z normalization.
                translate([0,0,plate_z])
                    rounded_block(plate_size,plate_thickness,plate_corner_r);
                for (p=screw_xy) translate([p[0],p[1],floor_h])
                    cylinder(h=base_post_top-floor_h,d=6.4,$fn=48);
                for (angle=snap_angles)
                    translate([coil_xy[0],coil_xy[1],floor_h])
                        rotate([0,0,angle-snap_sweep/2]) bobbin_snap();
                atom_seat();
                resistor_saddle();
                connector_support();
            }
            // Trim the rear-right support to the plate's rounded footprint.
            // Nothing on the plate may project into the shell's side walls.
            translate([0,0,plate_z-eps])
                rounded_block(plate_size,case_height,plate_corner_r);
        }
        for (p=concat(screw_xy,[atom_xy])) translate([p[0],p[1],-eps]) {
            cylinder(h=max(base_post_top,atom_z)+1,d=screw_clearance_d,$fn=32);
            cylinder(h=head_seat_z+eps,d=head_recess_d,$fn=48);
        }
        // Routing remains 0.5 mm deep, with 0.7 mm of plate below it.
        wire_channels();
        underside_engraving();
    }
}

module lid_model() {
    difference() {
        union() {
            difference() {
                // The roof and all four walls form one continuous upper shell.
                union() {
                    hull() {
                        rounded_block(case_size-0.8,eps,corner_r-0.4);
                        translate([0,0,0.4]) rounded_block(case_size,eps,corner_r);
                    }
                    translate([0,0,0.4]) rounded_block(case_size,roof_z+0.2,corner_r);
                    hull() {
                        translate([0,0,roof_z+0.6-eps]) rounded_block(case_size,eps,corner_r);
                        translate([0,0,case_height-eps]) rounded_block(case_size-0.8,eps,corner_r-0.4);
                    }
                }
                // Open underside; the plate's perimeter seam sits 0.4 mm inside.
                translate([0,0,-eps])
                    rounded_block(case_size-2*wall,roof_z+eps,corner_r-wall);
            }
            for (p=screw_xy) translate([p[0],p[1],lid_post_bottom]) {
                cylinder(h=roof_z-lid_post_bottom+eps,d=6,$fn=48);
                cap_d = (p[0]==50 && p[1]==48) ? 6.4 : 9;
                translate([0,0,roof_z-lid_post_bottom-1.5])
                    cylinder(h=1.5+eps,d1=6,d2=cap_d,$fn=48);
            }
            // Integral contact pad, preserving the v4 button-to-Atom gap.
            translate([button_xy[0],button_xy[1],button_contact_z])
                rotate([0,0,button_rotation]) translate([0,-4,0])
                    linear_extrude(roof_z-button_contact_z+eps)
                        rounded_rect([3,2],0.6);
        }
        port_cuts();
        // Preserve 0.8 mm clearance around the Atom's right-hand guide.
        translate([atom_xy[0]+12+atom_clearance-0.8,atom_xy[1]+3-0.8,lid_post_bottom-eps])
            cube([1.1+1.6,4+1.6,atom_z+1.8+0.8-lid_post_bottom+eps]);
        for (p=screw_xy) translate([p[0],p[1],lid_post_bottom-eps])
            cylinder(h=6.8,d=pilot_d,$fn=32);
        integrated_button_cuts();
        top_engraving();
    }
}

module bobbin_model() {
    difference() {
        union() {
            cylinder(h=bobbin_h,d=core_d);
            cylinder(h=lower_flange,d=flange_d);
            translate([0,0,lower_flange+winding_h]) cylinder(h=flare_h,d1=core_d,d2=flange_d);
            translate([0,0,bobbin_h-upper_flange]) cylinder(h=upper_flange,d=flange_d);
        }
        translate([0,0,-eps]) cylinder(h=bobbin_h+2*eps,d=core_d-2*core_wall);
        // Both wire ends enter the former and leave through the underside groove.
        for (z=[2.2,4.2]) rotate([0,0,45]) translate([38,0,z])
            rotate([0,90,0]) cylinder(h=7,d=1.4,$fn=32);
        rotate([0,0,45]) translate([37.5,-1.2,-eps]) cube([8,2.4,0.65+eps]);
        // Remove the upper inner lip; the lower locking shoulder remains.
        // The relief opens through the top without entering the winding channel.
        translate([0,0,snap_groove_z])
            cylinder(h=bobbin_h-snap_groove_z+eps,r=bobbin_inner_r+snap_groove_depth);
    }
}

module top_engraving() {
    translate([0,0,case_height-engraving_depth]) linear_extrude(engraving_depth+eps) {
        translate([0,-44]) offset(delta=label_weight_offset)
            text("watchsync",size=label_size,halign="center",valign="center",font="Segoe UI:style=Bold");
        for (direction=[-1,1]) hull() {
            translate([-marker_x_span/2,-direction*marker_x_span/2]) circle(d=marker_stroke,$fn=32);
            translate([marker_x_span/2,direction*marker_x_span/2]) circle(d=marker_stroke,$fn=32);
        }
    }
}

module integrated_button_cuts() {
    translate([button_xy[0],button_xy[1],roof_z-eps])
        rotate([0,0,button_rotation]) linear_extrude(lid_h+2*eps) union() {
            // Original flap orientation restored after the user's correction.
            difference() {
                difference() {
                    rounded_rect(button_size+[2*button_slit,2*button_slit],button_r+button_slit);
                    rounded_rect(button_size,button_r);
                }
                translate([-10,button_root_y]) square([20,10]);
            }
            for (x=[-1,1]) translate([x*(button_size[0]/2+button_slit/2),button_root_y])
                circle(d=0.9,$fn=32);
            rounded_rect(light_slot,light_slot[1]/2-0.02);
        }
    // A wider underside aperture leaves the existing LED unobstructed.
    translate([button_xy[0],button_xy[1],0]) hull() {
        translate([0,0,roof_z-eps]) linear_extrude(eps)
            rounded_rect(light_slot+[2,2],light_slot[1]/2+0.9);
        translate([0,0,case_height-0.25]) linear_extrude(eps)
            rounded_rect(light_slot,light_slot[1]/2-0.02);
    }
}

module atom_model() {
    // Original envelope model based on dimensions, not copied M5Stack geometry.
    difference() {
        rounded_block(atom_size[0],atom_size[2],atom_corner_r);
        translate([0,12.1,7.4]) rotate([90,0,0])
            linear_extrude(2.1) offset(r=0.8) square([7.6,1.4],center=true);
        translate([-4.2,11.1,0.7]) cube([8.4,1.1,4.2]);
        translate([10.9,-5.5,6.1]) rotate([0,90,0]) cylinder(h=1.3,d=1.8,$fn=32);
        translate([-1.4,-0.4,atom_size[2]-0.08]) cube([2.8,0.8,0.12]);
    }
}

module winding_model() {
    difference() {
        cylinder(h=winding_h,d=85);
        translate([0,0,-eps]) cylinder(h=winding_h+2*eps,d=core_d+0.1);
    }
}

module assembly_model(exploded=false) {
    color([0.18,0.20,0.22]) base_model();
    translate([coil_xy[0],coil_xy[1],floor_h+(exploded?8:0)]) {
        color([0.32,0.34,0.36]) bobbin_model();
        translate([0,0,lower_flange]) color([0.65,0.31,0.14]) winding_model();
    }
    translate([atom_xy[0],atom_xy[1],atom_z+(exploded?10:0)]) {
        color([0.12,0.14,0.16]) atom_model();
        // Built-in LED reference only; this is not an additional printed part.
        translate([-1.4,-0.4,atom_size[2]-0.07]) color([0.1,0.8,0.35]) cube([2.8,0.8,0.02]);
    }
    translate([0,0,exploded?28:0]) {
        color([0.22,0.24,0.26]) lid_model();
    }
}

// Individual printable parts are exported with their intended bed orientation.
if (part=="base") translate([0,0,-plate_z]) base_model();
else if (part=="lid") translate([0,0,case_height]) rotate([180,0,0]) lid_model();
else if (part=="bobbin") bobbin_model();
else if (part=="atom") atom_model();
else if (part=="winding") winding_model();
else assembly_model(part=="exploded");
