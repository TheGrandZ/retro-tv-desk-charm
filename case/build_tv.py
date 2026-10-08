import numpy as np, trimesh, math
from manifold3d import Manifold as M, CrossSection as CS, JoinType, set_circular_segments, Mesh, OpType
set_circular_segments(64)
OUT='stl/'   # run this script from the case/ folder
def rr(x0,x1,y0,y1,r):
    w,h=x1-x0,y1-y0; r=min(r,w/2-0.01,h/2-0.01)
    return CS.square((w-2*r,h-2*r),center=True).offset(r,JoinType.Round).translate(((x0+x1)/2,(y0+y1)/2))
def slab(cs,z0,z1): return cs.extrude(z1-z0).translate((0,0,z0))
def box(x0,x1,y0,y1,z0,z1): return M.cube((x1-x0,y1-y0,z1-z0)).translate((x0,y0,z0))
def xprism(cs_yz,x0,x1):
    """cross-section drawn in (y,z), extruded along x"""
    return cs_yz.extrude(x1-x0).transform([[0,0,1,x0],[1,0,0,0],[0,1,0,0]])
def U(lst): return M.batch_boolean(lst,OpType.Add)
def rounded_block(x0,x1,y0,y1,z0,z1,rxy,rf,rb,n=8):
    L=[]
    for i in range(n+1):
        t=math.pi/2*i/n
        d=rf*(1-i/n); z=z0+rf*(i/n); L.append(slab(rr(x0+d,x1-d,y0+d,y1-d,rxy-d),z,z+0.01))
        d=rb*(1-math.sin(t)); z=z1-rb*(1-math.cos(t)); L.append(slab(rr(x0+d,x1-d,y0+d,y1-d,rxy-d),z-0.01,z))
    return M.batch_hull(L)
def tm(man):
    m=man.to_mesh(); return trimesh.Trimesh(m.vert_properties[:,:3],m.tri_verts)
def tomf(t): return M(Mesh(vert_properties=np.asarray(t.vertices,dtype=np.float32),tri_verts=np.asarray(t.faces,dtype=np.uint32)))
FLIP=np.diag([1,-1,-1,1.0])          # proper 180deg rotation about x: puts the -z / +z face on the bed
def save(man,name,rot=None):
    t=tm(man)
    if rot is not None: t.apply_transform(rot)
    t.apply_translation(-t.bounds[0]*[0,0,1]); t.apply_translation([-t.bounds[:,0].mean(),-t.bounds[:,1].mean(),0])
    t.export(OUT+name+'.stl'); print(f'{name:28s}',t.extents.round(1),'watertight',t.is_watertight,'vol',round(t.volume/1000,1),'cm3')

# ---------------- reference geometry (case coords) ----------------
CL=0.25; PX,PY=45+CL,27.05+CL
ZCF=-4.91; ZFLOOR=-3.9; LIP=0.0; ZF=ZCF-LIP; ZB=14.0; ZRIM=9.4
WL,WR,WB,WT=3.5,26.0,3.5,5.0
X0,X1,Y0,Y1=-PX-WL,PX+WR,-PY-WB,PY+WT
RB=1.5                                   # small back fillet -> flat land for the cap to sit on
ZM=(ZF+ZB)/2
case=tomf(trimesh.load('original/2.8inch_Front_Case.stl'))   # download the original case yourself, see case/README.md

# ---------------- BODY (case merged in, one piece) ----------------
block=rounded_block(X0,X1,Y0,Y1,ZF,ZB,rxy=7,rf=1.2,rb=RB)
pockets=U([slab(rr(-PX,PX,-PY,PY,4+CL),ZRIM,ZB+1),                 # back cover + cap lip zone (full case outline)
           slab(rr(-44.3,44.3,-26.7,26.7,3.3),ZFLOOR,ZRIM+0.01)])    # stays inside the case wall thickness -> solid union
body=(block-pockets)+case
cut=[]
wx0,wx1,wy0,wy1=-26.8,32.0,-22.3,22.3
cut.append(M.batch_hull([slab(rr(wx0,wx1,wy0,wy1,4),ZFLOOR-0.05,ZFLOOR+0.1),              # CRT bevel, now ~38deg (was 58)
                         slab(rr(wx0-0.6,wx1+0.6,wy0-0.6,wy1+0.6,4.6),ZF-0.01,ZF+0.01)]))
g=4.5
cut.append(slab(rr(wx0-g-0.8,wx1+g+0.8,wy0-g-0.8,wy1+g+0.8,8)-rr(wx0-g,wx1+g,wy0-g,wy1+g,7.2),ZF-1,ZF+0.4))
cut.append(M.cylinder(0.2-(ZF-1),1.5).translate((39.35,-14.35,ZF-1)))                       # LDR hole, now reaches the sensor
kx=PX+WR/2
cut.append(slab(rr(PX+3.5,X1-3.5,Y0+3.5,Y1-3.5,3)-rr(PX+4.3,X1-4.3,Y0+4.3,Y1-4.3,2.2),ZF-1,ZF+0.4))   # 0.8 mm outline groove
# magnetic knob sockets: 7.3 mm bore for the knob shaft, 5x2 mm magnet seat at the bottom
KNOBY=(17.5,3.5); REC=ZF; BORE_D=7.3; BORE_DEPTH=5.9; MAG_D=5.2; MAG_H=2.2
for ky in KNOBY:
    cut.append(M.cylinder(BORE_DEPTH+1.6,BORE_D/2).translate((kx,ky,ZF-1)))            # bore, from the front
    cut.append(M.cylinder(MAG_H+0.01,MAG_D/2).translate((kx,ky,REC+BORE_DEPTH-0.01)))   # magnet seat
for i in range(5):
    y=-8.5-i*3.6; cut.append(slab(rr(kx-7,kx+7,y-0.8,y+0.8,0.8),ZF-1,ZF+1.8))
# covered connectors: blind relief pockets on the inside only (original hole sizes), solid skin outside
for (a,b,z0,z1) in ((23.8,31.6,1.1,6.0),(8.0,15.8,1.1,6.0)):          # two bottom pin connectors
    cut.append(box(a,b,-27.6,-25.4,z0,z1))
cut.append(box(-45.6,-43.4,-14.8,-5.3,0.6,5.1))                       # old micro-USB
cut.append(box(-45.6,-43.4,5.0,12.9,1.1,6.1))                         # P1 pin connector
# SD card: finger scoop that thins the wall to 1mm so the card sticks out, plus a card-sized guided slot
def yprism(cs_xz,y0,y1):   # cross-section drawn in (x,z), extruded along y
    return cs_xz.extrude(y1-y0).transform([[1,0,0,0],[0,0,1,y0],[0,1,0,0]])
SDY=-26.55                                                   # scoop floor (case wall outer face is -27.05)
cut.append(M.batch_hull([yprism(rr(-15.0,5.6,-0.5,5.7,1.5),SDY,SDY+0.01),
                         yprism(rr(-18.5,9.0,-1.5,9.2,2.5),Y0-0.5,Y0)]))
cut.append(M.batch_hull([yprism(rr(-12.2,2.8,1.5,3.7,0.6),-25.4,-25.39),             # original slot size at the socket
                         yprism(rr(-13.2,3.8,0.5,4.7,1.2),SDY-0.01,SDY+0.01)]))      # small lead-in funnel
# ONE wide service window on the USB side: JST(P1) + USB-C + micro-USB, sized for chunky plug overmolds
UCY,UCZ=-2.1,3.7            # USB-C centre, measured off the photos of the v1 print
# hidden wire channel: P1 plug -> up inside the wall -> rear battery bay
pass
# filament-dowel holes (antenna base on top, stand underneath) - unchanged from v1
for dx in (-8,8): cut.append(M.cylinder(4,0.95).rotate((-90,0,0)).translate((2.6+dx,Y1-3.6,ZM)))
for x in (-PX-WL/2+0.2, X1-6):
    top=Y0+3.0 if x<0 else Y0+6.0
    cut.append(M.cylinder(top-(Y0-1),0.95).rotate((90,0,0)).translate((x,top,ZM)))
# sockets for the cap's two locating pegs
PEG=[(67.8,15),(67.8,-15)]
for (px,py) in PEG: cut.append(M.cylinder(4,1.75).translate((px,py,ZB-3)))
body=body-U(cut)
# USB-C: plug-sized pocket outside, thin faceplate inside with a port-sized hole (hides the board)
SK0,SK1=-44.3,-43.5                                              # 0.8 mm faceplate = 2 print lines
PW,PH=14.0,8.5                                                   # pocket for the plug shell
HW,HH=10.4,5.0                                                   # hole for the port / plug tip
body=body+xprism(rr(UCY-PW/2-1,UCY+PW/2+1,UCZ-PH/2-1,UCZ+PH/2+1,3.5),SK0,SK1)
body=body-xprism(rr(UCY-PW/2,UCY+PW/2,UCZ-PH/2,UCZ+PH/2,3.2),X0-1,SK0)
body=body-xprism(rr(UCY-HW/2,UCY+HW/2,UCZ-HH/2,UCZ+HH/2,2.0),SK0-1,SK1+0.2)
save(body,'tv_body_v8')
fit=body^box(X0-1,9.5,Y0-1,Y1+1,ZF-1,ZB+1)
save(fit,'FIT_TEST_usb_and_sd_end')

# ---------------- REAR CAPS (designed in body coords so they are NOT mirrored) ----------------
def make_cap(D,battery):
    O=dict(x0=X0+RB,x1=X1-RB,y0=Y0+RB,y1=Y1-RB)
    c=4.0
    outer=M.batch_hull([slab(rr(O['x0'],O['x1'],O['y0'],O['y1'],5.5),ZB,ZB+D-c),
                        slab(rr(O['x0']+c,O['x1']-c,O['y0']+c,O['y1']-c,2),ZB+D-0.01,ZB+D)])
    floor=ZB+D-2.0
    cav_cs=rr(-PX+1.9,X1-RB-3.9,-PY+1.9,PY-1.9,2.4)
    half=CS.square((200,200)).translate((-200+44,-100))
    # locating lip (3 sides) - continues the thick wall, so nothing overhangs
    lip=slab(rr(-PX+0.3,PX-0.3,-PY+0.3,PY-0.3,3.95)^half,ZB-2.0,ZB+1.0)
    ribs=[M.cylinder(2.2,0.4).translate((x,y,ZB-1.7)) for (x,y) in
          [(-PX+0.3,20),(-PX+0.3,-20)]+[(x,s*(PY-0.3)) for x in (-30,0,30) for s in (-1,1)]]
    pegs=[M.cylinder(3.3,1.5).translate((px,py,ZB-2.3)) for (px,py) in PEG]
    cap=(outer+lip+U(ribs)+U(pegs))-slab(cav_cs,ZB-5,floor)
    sub=[]
    if battery:
        sub.append(box(-46.5,-42,3,15,ZB-3,ZB+0.5))                                  # lip notch: wires enter here
        sub.append(xprism(rr(-21,-7,floor-8.0,floor-0.5,1.5),X0-1,-43.0))             # USB-C charge port slot 14 x 7.5
        sub.append(box(-46.25,-43.0,15.4,24.6,floor-8.3,floor-3.7))                   # slide-switch body recess (SS12D00)
        sub.append(xprism(rr(16.75,23.25,floor-7.6,floor-4.4,0.8),X0-1,-46.0))        # switch lever slot
        for i in range(7):
            x=-14+i*8; sub.append(slab(rr(x-1.2,x+1.2,-12,12,1.2),ZB+D-0.6,ZB+D+1))   # vent look, not through (battery behind)
        fence=[box(8.8,10,-18,-8,floor-6,floor+0.01),box(8.8,10,8,18,floor-6,floor+0.01),
               box(22,50,18.1,19.3,floor-6,floor+0.01),box(22,50,-19.3,-18.1,floor-6,floor+0.01)]
        cap=cap+U(fence)                                                               # 103450 cell sits at x 10..62, y +-17.5
    else:
        for i in range(7):
            x=-14+i*8; sub.append(slab(rr(x-1.2,x+1.2,-12,12,1.2),floor-1,ZB+D+1))
    return cap-U(sub)
cap_plain=make_cap(8.0,False); save(cap_plain,'tv_rear_cap_v8',FLIP)


# ---------------- STAND (deeper stance so the battery can't tip it) ----------------
fz0,fz1=ZF+1.2,ZB+8.0; fx0,fx1=X0+3,X1-3; T=3.0; L=20
frame=slab(rr(fx0,fx1,fz0,fz1,3),0,T)-slab(rr(fx0+6,fx1-6,fz0+2.2,fz1-3.5,2),-1,T+1)
legs=[]
for (lx,sx) in ((fx0+3,-1),(fx1-3,1)):
    for (lz,sz) in ((fz0+2,-1),(fz1-2,1)):
        legs.append(M.cylinder(L,2.6,1.6).rotate((sz*-9,sx*12,0)).translate((lx,lz,T-0.5)))
        legs.append(M.sphere(1.9).translate((lx+sx*L*math.sin(math.radians(12)),lz+sz*L*math.sin(math.radians(9)),T-0.5+L*0.97)))
stand=frame+U(legs)
for x in (-PX-WL/2+0.2, X1-6): stand=stand-M.cylinder(T+2,0.95).translate((x,ZM,-1))
save(stand,'tv_stand_v8')

# ---------------- MAGNETIC KNOB (print 2, face down) ----------------
SH_D=6.8; RING=0.3; SH_L=BORE_DEPTH-0.4+RING          # shaft: 0.25 mm radial play in the bore, stops 0.4 mm short of the socket magnet
knob=M.cylinder(6,5.7,6.5)
notches=[M.cylinder(8,0.7).translate((6.6*math.cos(a),6.6*math.sin(a),-1)) for a in np.linspace(0,2*math.pi,18,endpoint=False)]
knob=knob-U(notches)
knob=knob-box(-0.6,0.6,1.5,6.0,-1,0.7)                                   # pointer groove on the face
knob=knob+(M.cylinder(0.3,5.0)-M.cylinder(1,4.0).translate((0,0,-0.3))).translate((0,0,5.99))   # low glide ring so only a thin ring rubs
knob=knob+M.cylinder(SH_L+0.01,SH_D/2).translate((0,0,5.99))             # shaft
knob=knob-M.cylinder(2.2,MAG_D/2).translate((0,0,6+SH_L-2.1))            # 5x2 magnet pocket in the shaft tip
save(knob,'tv_knob_magnetic_x2')
# small coupon to test magnet fit + spin before printing the whole body
coupon=body^box(kx-9.5,kx+9.5,KNOBY[0]-8,KNOBY[0]+9,ZF-1,ZF+10.5)
save(coupon,'FIT_TEST_knob_socket')
