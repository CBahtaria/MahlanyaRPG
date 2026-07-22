# Blender Character & Prop Pipeline

Blender 4.x → UE5.4 via Send to Unreal (ueify) or FBX export. All assets must pass historical accuracy gate before import into the project.

---

## Toolchain

| Step | Tool | Output |
|------|------|--------|
| Modelling | Blender 4.x | `.blend` source |
| Rigging | Blender Rigify → UE5 skeleton retarget | `.blend` |
| UV unwrap | Blender UV Editor | UV map in `.blend` |
| Textures | Blender bake → Substance Painter (optional) | `_D.png`, `_N.png`, `_ORM.png` |
| Export | FBX (Binary, UE5 preset) | `.fbx` + `.png` textures |
| Import | UE5 FBX importer | `.uasset` mesh + material |
| LOD | Nanite (static) / UE5 LOD Group (skeletal) | auto |

Source `.blend` files live in `pipeline/art/source/`. Never check in `.fbx` — export on demand.

---

## Emabutfo Warrior Armor (Phase 11 reference design)

**Cultural basis**: Swazi emabutfo (age regiment) ceremonial armor circa 1880–1895. Primary references: Swazi ethnographic records in `docs/lore/emabutfo-regalia.md` (source: historical photographs + anthropological records, no post-colonial synthetic materials).

**Design constraints (non-negotiable)**:
- Animal hide: leopard, lion, or cattle skin — cowhide shields (`umgoloza`) standard for most regiments; leopard reserved for royalty and senior izinduna
- Feathered headgear: lourie (purple-crested turaco) feathers for royalty, widowbird tail feathers common
- No metal plate armor — pre-industrial Eswatini did not have iron-plate smithing tradition; ring mail and chainmail are NOT appropriate
- Spear (`umkhonto`) and knobkierie (`likhwili`) as primary weapons; short stabbing spear (`iklwa`) for close combat regiments
- Ochre and white clay face markings for initiation regiments
- Grass ankle rattles (`ligqwesha`) — visible and modelled, not implied

**Mesh spec**:
- LOD0 (hero): ~8,000 triangles character body; ~3,000 tris per armor piece (chest, arms, legs separate meshes)
- LOD1: ~4,000 tris body + ~1,500 per piece (Nanite handles static; skeletal uses this)
- Skin: 4-bone influence per vertex, max
- Skeleton: matches `SK_MahlanyaHero_Skeleton` rig — reuse bone names exactly
- Scale: 180 cm character height at neutral A-pose in Blender before export

**Texture maps** (2K for hero, 1K for NPCs):
- `T_EmabutfoArmor_D` — diffuse/albedo
- `T_EmabutfoArmor_N` — normal map (OpenGL convention, flip Y on export for UE5)
- `T_EmabutfoArmor_ORM` — packed: R=Occlusion, G=Roughness, B=Metallic (Metallic always 0 for hide)

---

## Usuthu Canoe (mokoro) — Phase 11

**Cultural basis**: Dugout canoe (`umkhumbi`) — single-log hull, flattened bottom, no keel. Paddled with long pole or flat paddle depending on river depth. Width ~70 cm, length ~3.5 m typical for the Usuthu tributary size.

**Mesh spec**:
- Triangle count: ~1,200 tris (static mesh, Nanite enabled)
- Single UV island — no UV overlapping
- Pivot point: geometric centre of hull at waterline
- Asset metadata tag: `hull_type: dugout_mokoro` (required by CI gate)

**Blender workflow**:
1. Start from cylinder primitive, diameter 70 cm, length 3.5 m
2. Loop cuts to define bow and stern taper
3. Flatten bottom face loop — boats of this type have near-flat undersides from natural log geometry
4. Gouge interior cavity — 25 cm deep, leaving 4 cm wall thickness
5. Apply Subdivision Surface modifier at level 2 before export (apply, do not leave as modifier)
6. UV unwrap: Smart UV Project at 45° angle margin 0.02
7. Export FBX: `SM_UsuthuCanoe.fbx`, Scale = 1.0, Forward = -Y, Up = Z (UE5 defaults)

**Rigging**: none — static mesh with UE5 `UBuoyancyComponent`.

---

## Export Checklist

Before exporting any asset to UE5:

- [ ] Apply all transforms (Ctrl+A → All Transforms) in Blender
- [ ] Remove doubles (Mesh → Merge by Distance, threshold 0.001 m)
- [ ] Check normals (Overlay → Face Orientation — all blue)
- [ ] Asset metadata tag set in custom properties panel
- [ ] File naming: `SM_` prefix for static, `SK_` for skeletal, `T_` for textures
- [ ] Run `scripts/historical_accuracy_check.py` after import — must return 0 anachronisms

---

## Source File Locations

```
pipeline/art/
├── blender-pipeline.md         ← this file
├── source/
│   ├── characters/
│   │   ├── SK_MahlanyaHero.blend
│   │   └── SK_Ntombi_Elder.blend
│   ├── props/
│   │   ├── SM_UsuthuCanoe.blend
│   │   └── SM_EmabutfoShield.blend
│   └── armor/
│       └── SK_EmabutfoArmor_Set.blend
└── textures/
    ├── T_EmabutfoArmor_D.png
    ├── T_EmabutfoArmor_N.png
    └── T_EmabutfoArmor_ORM.png
```

Source `.blend` files are Git LFS tracked (`.gitattributes` already covers `*.blend`).
