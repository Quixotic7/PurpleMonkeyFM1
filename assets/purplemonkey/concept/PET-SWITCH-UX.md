# Musical worlds + pet announcement — concept revision

Static visual mockups only; no firmware, animation, or audio sample implementation.

## Pet selection proposal

SELECT cycles Monkey, Cat, Dog, Llama. Pet selection is separate from DRUMS/SYNTH and from backing-pattern start/stop. Every animal is available in either mode. The established instruments are visual personalities, not mode restrictions. For this revision, SELECT replaces its previous kit/instrument browsing role; each pet can have a curated default sound for each mode. Brightness and length still shape the active sound. This mapping is proposed, not finalized.

When selection changes, show the new animal immediately with a large animal name. Give the title one gentle pop/fade, not repeated flashing. Keep it visible for approximately 0.8–1.2 seconds, then return to the normal music scene. Keyboard input remains active throughout. The backing beat and its clock continue unchanged.

A short friendly recorded voice says exactly “Monkey!”, “Cat!”, “Dog!” or “Llama!”. These are proposed sample scripts, not generated audio files. Use one voice, consistent perceived loudness, a gentle onset, and the same master-volume/output ceiling as the music. Briefly lower accompaniment if needed for intelligibility; do not stop it.

For rapid knob turning, update the visual selection immediately but wait roughly 250 ms after the last change before speaking. Cancel pending announcements; never queue a list of animal names. If a new selection arrives during speech, fade the old cue and announce only the settled selection. Do not repeat a name when selection has not changed. Adult configuration can disable speech. Exact timings need listening/play testing later.

## Reactive environment

Maintain dark purple, teal, indigo and plum base environments. Slowly interpolate base hue over musical phrases. Kick creates a localized soft color pool; snare makes a hill bounce; hats add sparse dots; synth taps release bubbles. Sustained notes grow ribbons and flowers. A handful of notes gradually brings the full band and garden into view, without requiring greater loudness.

Limit decorative density and preserve character silhouettes. Cat retains golden eyes, pink nose, tapered white forehead blaze and white bib from the user's photo. White dog remains distinct from cream llama. No full-screen white flashes. When the beat stops, rhythmic ripples settle; manual playing continues to animate its own responses.

The generated six-screen sheet is visual direction, not exact 240×240 pixel artwork. Fine hatching and full-band spacing need simplification in a future pixel-scale pass. Existing Designer exports still document the previous control mapping; this note records the proposed SELECT change for review.
