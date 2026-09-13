
# 1. export.mjs: the boundary-edge count IS the perimeter in pixels; no halving.
p = 'tools/export.mjs'
s = open(p, encoding='utf-8').read()
old = "  const perimeter_m = (boundary / 2) / raster.px_per_m;"
new = "  const perimeter_m = boundary / raster.px_per_m;"
assert s.count(old) == 1, 'perimeter'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('perimeter fixed')

# 2. kestrel: bands clear of each other, every part at or over the 1.5 m floor.
p = 'tools/models/kestrel.ts'
s = open(p, encoding='utf-8').read()

old = """  box(group, ochre, [2 * HW - 0.4, 3.5, 0.5], [0, at(0.115), DECK + 0.8]);
  box(group, teal, [2 * HW - 0.4, 3.5, 0.5], [0, at(0.145) + 1.75, DECK + 0.8]);
  for (const side of [-1, 1]) {
    box(group, ochre, [0.5, 3.5, 2 * DECK - 0.6], [side * (HW - 0.1), at(0.115), 0]);
    box(group, teal, [0.5, 3.5, 2 * DECK - 0.6], [side * (HW - 0.1), at(0.145) + 1.75, 0]);
  }"""
new = """  box(group, ochre, [2 * HW - 0.4, 3.5, 1.5], [0, 18.2, DECK + 1.2]);
  box(group, teal, [2 * HW - 0.4, 3.5, 1.5], [0, 14.5, DECK + 1.2]);
  for (const side of [-1, 1]) {
    box(group, ochre, [1.5, 3.5, 2 * DECK - 0.6], [side * (HW - 0.1), 18.2, 0]);
    box(group, teal, [1.5, 3.5, 2 * DECK - 0.6], [side * (HW - 0.1), 14.5, 0]);
  }"""
assert s.count(old) == 1, 'bands'
s = s.replace(old, new)

old = """  for (const [id, f, s] of [['pdc.fwd', 0.20, 0.25], ['pdc.aft', 0.28, -0.25]] as const) {
    const x = s * HW, y = at(f);
    cylinder(group, frame, 1.15, 1.25, 0.9, [x, y, DECK + 0.45], 14);
    hp(group, id, [x, y, DECK + 1.0]);
  }
  cylinder(group, frame, 1.15, 1.25, 0.9, [0, at(0.42), -DECK - 0.45], 14);
  hp(group, 'pdc.ventral', [0, at(0.42), -DECK - 1.0]);"""
new = """  for (const [id, f, s] of [['pdc.fwd', 0.20, 0.25], ['pdc.aft', 0.28, -0.25]] as const) {
    const x = s * HW, y = at(f);
    cylinder(group, frame, 1.15, 1.25, 1.5, [x, y, DECK + 0.75], 14);
    hp(group, id, [x, y, DECK + 1.5]);
  }
  cylinder(group, frame, 1.15, 1.25, 1.5, [0, at(0.42), -DECK - 0.75], 14);
  hp(group, 'pdc.ventral', [0, at(0.42), -DECK - 1.5]);"""
assert s.count(old) == 1, 'drums'
s = s.replace(old, new)

old = "  box(group, dark, [6.4, at(0.30) - at(0.44), 0.5], [0, (at(0.30) + at(0.44)) / 2, DECK - 0.8]);"
new = "  box(group, dark, [6.4, at(0.30) - at(0.44), 1.5], [0, (at(0.30) + at(0.44)) / 2, DECK - 1.1]);"
assert s.count(old) == 1, 'bay floor'
s = s.replace(old, new)

old = """    box(group, dark, [1.6, 1.6, 1.6], [x, y, 0]);
    const nozzle = cylinder(group, metal, 0.5, 0.5, 0.8, [x + side * 1.1, y, 0], 10);
    nozzle.rotation.z = Math.PI / 2;
    effect_cone(group, flames, { name: 'rcs-jet', radius: 0.55, length: 4.5, pos: [x + side * 3.1, y, 0], rot: [0, 0, -side * Math.PI / 2] });"""
new = """    box(group, dark, [1.6, 1.6, 1.6], [x, y, 0]);
    // The nozzle cross the plan-04 hull modelled is map content now: four 0.5 m nozzles were
    // three-pixel stumps at the home framing (J6). The sim-resolved jet cone stays.
    effect_cone(group, flames, { name: 'rcs-jet', radius: 0.55, length: 4.5, pos: [x + side * 3.1, y, 0], rot: [0, 0, -side * Math.PI / 2] });"""
assert s.count(old) == 1, 'rcs'
s = s.replace(old, new)

old = """  dock(group, 'A', [-HW, at(0.46), 0], Math.PI / 2, 'M');
  cylinder(group, frame, 2.0, 2.1, 0.6, [-HW - 0.2, at(0.46), 0], 16).rotation.z = Math.PI / 2;"""
new = """  dock(group, 'A', [-HW, at(0.46), 0], Math.PI / 2, 'M');
  cylinder(group, frame, 2.0, 2.1, 1.6, [-HW - 0.7, at(0.46), 0], 16).rotation.z = Math.PI / 2;"""
assert s.count(old) == 1, 'dock ring'
s = s.replace(old, new)

old = """  for (const side of [-1, 1]) for (const f of [0.32, 0.68]) {
    box(group, metal, [1.6, 2.4, 1.5], [side * 3.4, at(f), -DECK - 1.5]);
    box(group, dark, [2.2, 2.4, 0.5], [side * 3.4, at(f), -DECK - 2.2]);
  }"""
new = """  for (const side of [-1, 1]) for (const f of [0.32, 0.68]) {
    box(group, metal, [1.6, 2.4, 1.5], [side * 3.4, at(f), -DECK - 1.5]);
    box(group, dark, [2.2, 2.4, 1.5], [side * 3.4, at(f), -DECK - 2.6]);
  }"""
assert s.count(old) == 1, 'pads'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('kestrel fixed')
