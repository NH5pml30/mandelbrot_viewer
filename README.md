# mandelbrot_viewer
Mandelbrot set viewer program: uses QT for drawing with all available logical cores in parallel. Draws primitives (`superpixels` - 256x256 pixels squares), trying to reuse rendered squares inside a virtual screen with 1.5x size of the real one.

If compiling with Visual Studio, you need to manually set QT paths in `CMakeSettings.json` file.

## Controls
Mouse drag for pan, mouse wheel for zoom.

## Settings
Draft Mip-Map Level:
 - if set to 0, (blocking) draws first screen draft in 1:1 scale;
 - if set to 8, (blocking) draws first screen draft in 1:256 (1 pixel for every 256 real pixels in 1d) scale;
 - anything in between is scaled in powers of 2 (1:2^{level} resolution).
