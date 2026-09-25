-- Wall push clip tests exported from 3d_model_viewer (js/wall_push_clips.js).
-- Run with tools/wall_clip_tester.lua in BizHawk.
return {
  game = "MM", map = "Laundry Pool", form = "Deku (14)",
  radius = 14, checkHeight = 26.8, numPolygons = 400,
  walls = {
    [27] = {v={{-1431, 0, 875}, {-1431, 400, 435}, {-1431, 0, 435}}, n={-32767, 0, 0}, d=-1431},
    [71] = {v={{-1653, 120, 341}, {-1431, 120, 435}, {-1431, 0, 435}}, n={12776, 0, -30174}, d=959},
  },
  tests = {
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1431.3901, -1.089794, 432.78842}, next={-1430.8049, -1.089794, 435.73077}, expect={-1445, -1.089794, 434.75}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1434.1215, -10.244081, 419.05743}, next={-1430.8049, -10.244081, 435.73077}, expect={-1445, -10.244081, 434.7501}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1431.3901, -1.2473298, 432.53842}, next={-1430.8049, -1.2473298, 435.48077}, expect={-1445, -1.2473298, 434.5}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1434.1215, -10.287227, 418.80743}, next={-1430.8049, -10.287227, 435.48077}, expect={-1445, -10.287227, 434.5001}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1431.3901, -1.404847, 432.28842}, next={-1430.8049, -1.404847, 435.23077}, expect={-1445, -1.404847, 434.25}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1434.1215, -10.330374, 418.55743}, next={-1430.8049, -10.330374, 435.23077}, expect={-1445, -10.330374, 434.2501}},
    {group=1, kind="extended", type="cross", pusher=27, crossed=71, prev={-1431.7804, -2.9077485, 430.07687}, next={-1430.8049, -2.9077485, 434.98077}, expect={-1445, -2.9077485, 434.00012}},
  },
}
