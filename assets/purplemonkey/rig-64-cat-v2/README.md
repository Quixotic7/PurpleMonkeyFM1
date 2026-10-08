# Photo-based asymmetric cat revision

New cat option based on the supplied IMG_1800.jpg reference. Broader rounded face; narrow uneven white blaze and black masks; relaxed unequal olive-gold eyes; mottled pink nose; independent left/right ears; wider uneven white bib; black tail without the earlier white tip. Other pets are unchanged.

The cat folder contains 21 rig parts, 35 sprites in exact64px atlas cells, separately registered eyes/mouth/nose/ear variations, neutral assembly and expression previews. Articulated arms and legs and the xylophone are retained. Left and right ear art is independently generated, not mirrored. Hand and limb art is inherited from the prior rig, so this revision focuses its asymmetry on the face, ears and coat markings.

Use this cat directory in place of rig-64-modular/cat when choosing the revised likeness. Do not mix its head or face layers with the old registration. The 24-slot capacity and per-part facial texture-swap integration requirements in rig-64-modular/README.md still apply. This is saved artwork, not a flashed firmware change. Earlier cat art is preserved for comparison.

Source was generated with built-in image_gen using the supplied photo. Source specification: modular tuxedo cat closely matching the photo, broad cheeks, crooked narrow blaze, uneven black masks, almond olive-gold eyes and unequal eyelids, mottled pink nose, crooked friendly smile; independent left/right relaxed/perked/drooped/flicked ears; blank earless head; paired eye variations; nose and mouth variants; asymmetrical body bib and black tail. Transparent background and simple storybook ink were requested. The generated source is more textured than the earlier cartoon set; the runtime assets are reduced to <=31 colors with binary alpha.

Original generated image: exec-164c2321-a0c4-40a4-8da9-fe93cdbfcf71.png, copied to source/cat-face-source.png. Manual crop rectangles are in source/crop-grid.json. Extraneous pale muzzle backgrounds were removed from feature cutouts during extraction. Rebuild with tools/art/build_cat_asymmetric.py, using the preserved prior modular cat as the limb/rig source.

Validation: dimensions <=64px; binary transparency; <=31 colors across all body/facial sprites; pivot/attachment bounds; exact atlas round trips. Neutral/default rig export was checked with MAXP24 in an isolated Python process, followed by Clang C syntax checking. Actual exporter/runtime were not modified or hardware tested.
