# Changelog

## [0.1.0] 2021-01-29

Hello world! Here's the first release.

It's functional but barebones and the GUI code definitely needs some improvements.

The Aseprite/Squint interoperation "protocol" will most likely evolve in the future, notably to add versioning (to avoid bad surprises in the future).

Feel free to send issues with bug reports or features request, I'll see what I can do to fulfill them.

Have a nice day!

## [0.1.1] 2021-02-06

## Added
- xBR shaders have now a corner mode selection setting that alters the way they match and interpolate the pixels.

## Fixed
- xBR-lv2 now compile and works on Intel HD iGPU (https://github.com/Eiyeron/squint/issues/1)
- The masking color, a magenta with out-of-bounds values, has been tweaked to avoid leaking into the sprite.
- The "Save picture" shortcut wasn't working unless the GUI was shown. This was fixed.

[0.1.0]: https://github.com/Eiyeron/squint/releases/tag/v0.1.0
[0.1.1]: https://github.com/Eiyeron/squint/releases/tag/v0.1.1