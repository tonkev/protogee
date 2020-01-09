![Demo](Demo.gif)

This global illumination renderer achieves a smoother framerate with minor quality deficits by only computing part of the indirect illumination each frame.
This is done by storing previous frames of indirect illumination and reprojecting these frames to fit the current frame.
More details are available in the Report pdf.