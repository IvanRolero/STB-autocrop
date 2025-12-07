# Autocrop
Simple autocrop program using a grayscale threshold and the `stb_image` library.  
**Warning:** This program overwrites the image being cropped.

## Usage:

autocrop [-t threshold (default 75)] <image1> [<image2> ...]
- **Lower values** = more sensitive (detects lighter content)
- **Higher values** = less sensitive (detects darker content only)

## Examples

### Crop with default threshold (75)

autocrop image1.jpg image2.png

### Crop with custom threshold (more sensitive)

autocrop -t 50 image3.jpg

### Crop multiple images

autocrop -t 100 *.jpg *.png
