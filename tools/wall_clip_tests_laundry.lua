-- Wall push clip tests exported from 3d_model_viewer (js/wall_push_clips.js).
-- Run with tools/wall_clip_tester.lua in BizHawk.
return {
  game = "MM", map = "Laundry Pool", form = "Deku (14)",
  radius = 14, checkHeight = 26.800001, numPolygons = 400,
  walls = {
    [27] = {v={{-1431, 0, 875}, {-1431, 400, 435}, {-1431, 0, 435}}, n={-32767, 0, 0}, d=-1431},
    [71] = {v={{-1653, 120, 341}, {-1431, 120, 435}, {-1431, 0, 435}}, n={12776, 0, -30174}, d=959},
    [64] = {v={{-1431, 0, 435}, {-1653, -120, 341}, {-1653, 120, 341}}, n={12776, 0, -30174}, d=959},
  },
  tests = {
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1434.0957, -10.242371, 418.9966}, next={-1430.8171, -17.74237, 435.73276}, yaw=0x07E8, speed=11.369783, expect={-1445, -17.74237, 434.79932}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1432.6425, -9.446444, 419.61194}, next={-1430.9017, -16.946444, 435.74475}, yaw=0x0467, speed=10.817935, expect={-1445, -16.946444, 434.834}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1434.1215, -10.287227, 418.80743}, next={-1430.8021, -17.787228, 435.48007}, yaw=0x0800, speed=11.333336, expect={-1445, -17.787228, 434.48618}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1432.7322, -9.495471, 419.57394}, next={-1430.8907, -16.995472, 435.49356}, yaw=0x04B5, speed=10.684154, expect={-1445, -16.995472, 434.54904}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1434.1215, -10.330374, 418.55743}, next={-1430.8021, -17.830374, 435.23007}, yaw=0x0800, speed=11.333336, expect={-1445, -17.830374, 434.23618}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1432.8219, -9.544635, 419.53595}, next={-1430.8832, -17.044636, 435.24307}, yaw=0x0505, speed=10.550943, expect={-1445, -17.044636, 434.2966}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1433.9584, -10.167154, 419.0548}, next={-1430.8126, -17.667154, 434.9818}, yaw=0x07F6, speed=10.823453, expect={-1445, -17.667154, 434.0331}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1432.9117, -9.593867, 419.49796}, next={-1430.8782, -17.093868, 434.9924}, yaw=0x0557, speed=10.418341, expect={-1445, -17.093868, 434.06418}},
    {group=2, kind="extended", type="cross", pusher=27, crossed=64, prev={-1441.4767, -14.285191, 415.8714}, next={-1430.5079, -21.78519, 434.8704}, yaw=0x155A, speed=14.625449, expect={-1445, -21.78519, 434.0181}},
  },
}
