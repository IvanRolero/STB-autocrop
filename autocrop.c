#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>

//Load STB Image Library Read and write modules
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static inline unsigned char to_grayscale(const unsigned char* p, int channels) {
    if (channels >= 3)
        return (unsigned char)((p[0] * 0.299f) + (p[1] * 0.587f) + (p[2] * 0.114f));
    return p[0]; // grayscale image
}

unsigned char* autocrop_binarized(
    const unsigned char* data,
    int width, int height, int channels,
    int* out_width, int* out_height,
    unsigned char bin_threshold
) {
    int min_x = width, max_x = 0;
    int min_y = height, max_y = 0;
    bool found = false;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned char* pixel = data + (y * width + x) * channels;
            unsigned char gray = to_grayscale(pixel, channels);

            if (gray < bin_threshold) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                found = true;
            }
        }
    }

    if (!found) return NULL;

    int new_width = max_x - min_x + 1;
    int new_height = max_y - min_y + 1;

    *out_width = new_width;
    *out_height = new_height;

    unsigned char* result = malloc(new_width * new_height * channels);
    if (!result) return NULL;

    for (int y = 0; y < new_height; ++y) {
        errno_t err = memcpy_s(
            result + y * new_width * channels,
            new_width * channels,
            data + ((min_y + y) * width + min_x) * channels,
            new_width * channels
        );
        if (err != 0) {
            free(result);
            return NULL;
        }
    }

    return result;
}

static bool str_iequals(const char* a, const char* b, size_t max_len) {
    if (a == NULL || b == NULL) return false;
    
    for (size_t i = 0; i < max_len; ++i) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
            return false;
        }
        if (a[i] == '\0' || b[i] == '\0') {
            return (a[i] == '\0' && b[i] == '\0');
        }
    }
    return true;
}

bool has_extension(const char* filename, const char* ext) {
    if (filename == NULL || ext == NULL) return false;
    
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return false;
    
    return str_iequals(dot + 1, ext, 255);
}
//Function to get jpeg dpi from original image
bool get_jpeg_dpi(const char* filename, int* x_dpi, int* y_dpi, int* unit) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return false;
    
    unsigned char header[18];
    if (fread(header, 1, 18, fp) != 18) {
        fclose(fp);
        return false;
    }

    // Check for JPEG markers and JFIF identifier
    if (header[0] != 0xFF || header[1] != 0xD8 ||  // SOI
        header[2] != 0xFF || header[3] != 0xE0 ||  // APP0
        memcmp(header + 6, "JFIF\0", 5) != 0) {    // JFIF identifier
        fclose(fp);
        return false;
    }

    *unit = header[13];
    *x_dpi = (header[14] << 8) | header[15];
    *y_dpi = (header[16] << 8) | header[17];
    fclose(fp);
    return true;
}

//Hack to write dpi metadata to the cropped image
bool inject_jpeg_dpi(const char* filename, int x_dpi, int y_dpi, int unit) {
    FILE* fp = fopen(filename, "r+b");
    if (!fp) return false;

    unsigned char header[18];
    if (fread(header, 1, 18, fp) != 18) {
        fclose(fp);
        return false;
    }

    // Verify we're modifying a JFIF JPEG
    if (header[0] != 0xFF || header[1] != 0xD8 ||
        header[2] != 0xFF || header[3] != 0xE0 ||
        memcmp(header + 6, "JFIF\0", 5) != 0) {
        fclose(fp);
        return false;
    }

    // Overwrite DPI fields
    fseek(fp, 13, SEEK_SET);
    fputc(unit, fp);
    fputc((x_dpi >> 8) & 0xFF, fp);
    fputc(x_dpi & 0xFF, fp);
    fputc((y_dpi >> 8) & 0xFF, fp);
    fputc(y_dpi & 0xFF, fp);

    fclose(fp);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s [-t threshold] <image1> [<image2> ...]\n", argv[0]);
        return 1;
    }

    unsigned char threshold = 75;
    int start_index = 1;

    // Check for optional -t argument
    if (argc > 2 && argv[1] != NULL && argv[2] != NULL && 
        strncmp(argv[1], "-t", 3) == 0) {
        char* endptr;
        long t = strtol(argv[2], &endptr, 10);
        if (endptr == argv[2] || *endptr != '\0') {
            fprintf(stderr, "Invalid threshold value: %s\n", argv[2]);
            return 1;
        }
        if (t < 0 || t > 255) {
            fprintf(stderr, "Threshold must be between 0 and 255.\n");
            return 1;
        }
        threshold = (unsigned char)t;
        start_index = 3;
    }

    if (start_index >= argc) {
        fprintf(stderr, "No input files provided.\n");
        return 1;
    }

    for (int i = start_index; i < argc; ++i) {
        if (argv[i] == NULL) continue;
        
        const char* filename = argv[i];
        int w, h, c;
        unsigned char* image = stbi_load(filename, &w, &h, &c, 0);
        if (!image) {
            fprintf(stderr, "Failed to load image: %s\n", filename);
            continue;
        }

        // Read original DPI (JPEG only)
        int x_dpi = 0, y_dpi = 0, unit = 0;
        bool has_dpi = false;
        if (has_extension(filename, "jpg") || has_extension(filename, "jpeg")) {
            has_dpi = get_jpeg_dpi(filename, &x_dpi, &y_dpi, &unit);
        }

        int new_w, new_h;
        unsigned char* cropped = autocrop_binarized(image, w, h, c, &new_w, &new_h, threshold);
        if (!cropped) {
            fprintf(stderr, "No content found to crop in: %s\n", filename);
            stbi_image_free(image);
            continue;
        }

        bool success = false;
        if (has_extension(filename, "png")) {
            success = stbi_write_png(filename, new_w, new_h, c, cropped, new_w * c);
        } else if (has_extension(filename, "jpg") || has_extension(filename, "jpeg")) {
            success = stbi_write_jpg(filename, new_w, new_h, c, cropped, 90);
            if (success && has_dpi) {
                inject_jpeg_dpi(filename, x_dpi, y_dpi, unit);
            }
        } else {
            fprintf(stderr, "Unsupported file extension for: %s\n", filename);
        }

        if (!success) {
            fprintf(stderr, "Failed to write cropped image: %s\n", filename);
        }

        stbi_image_free(image);
        free(cropped);
    }

    return 0;
}
