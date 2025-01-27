# Degeratu Razvan Andrei 323CA
## Overview
This is my implementation of an image processing program for the last homework.
The application handles PGM/PPM formats and includes various image manipulation features.
>The most challenging part was the filter implementations and memory management.

## Supported Image Formats

ASCII: P2 (grayscale), P3 (color)
Binary: P5 (grayscale), P6 (color)

## Main Features

I/O Operations

Load both ASCII and binary formats
Save with format conversion option

# Image Manipulation

Area selection using coordinates
Cropping functionality
90-degree rotation (works on both full image and selections)
Various filters implementation


# Advanced Filters

Edge Detection using 3x3 Laplacian kernel (interesting, used for finding boundaries).
Sharpening with 3x3 Laplacian enhancement filter.
Basic Blur using 3x3 averaging
Gaussian Blur for more precise result.


# Histogram Tools

Interactive ASCII visualization (using *)
Custom bin size support
Equalization for improving contrast
Works on grayscale images


Implementation Details
Used an image_t struct to keep track of:

Image dimensions and format
Pixel data
Current selection
channel info

# Data Structures:

Dynamic 2D arrays for efficient pixel access
Enums to handle different formats and filters
Custom kernels for each filter type

# Notes

Implemented checking for allocation/deallocation
Added checks for memory leaks
Used valgrind  for tgrading

# Filter Edge Cases

Added border handling for filters
Implemented pixel value clamping

# Rotation Algorithm

Tricky part was handling the selection rotation
Had to ensure square selections for rotation

# Resources Used

Course materials
Stack Overflow and Wikipedia for understanding kernels
Valgrind/gdb for finding memory leaks/ identyfying breaking points in the program
Note: I used some comments for a better understanding of the code for better understanding. 
