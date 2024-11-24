#pragma once
/*
This file contains useful function and definitions.
Do not ever edit this file - it will not be uploaded for evaluation.
If you want to modify any of the functions here (e.g. extend triangle test to quads),
copy the function "your_code_here.h" and give it a new name.
*/
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#ifdef _OPENMP
// Only if OpenMP is enabled.
#include <omp.h>
#endif

#include <framework/image.h>

/// <summary>
/// Structure of an image with 3 planes.
/// </summary>
template<typename T>
struct ImagePlane3 {
    T X;
    T Y;
    T Z;

    T& operator[](size_t index)
    {
        switch (index) {
        case 0:
            return X;
        case 1:
            return Y;
        case 2:
            return Z;
        default:
            throw std::out_of_range("Index out of range");
        }
    }

    const T& operator[](size_t index) const
    {
        switch (index) {
        case 0:
            return X;
        case 1:
            return Y;
        case 2:
            return Z;
        default:
            throw std::out_of_range("Index out of range");
        }
    }
};


/// <summary> Scalar image </summary>
using ImageFloat = Image<float>;
/// <summary> 3-channel image stored in attrtibute order </summary>
using ImageVec3 = Image<glm::vec3>;
/// <summary> 3-channel image stored in plane order </summary>
using ImageFloatPlane3 = ImagePlane3<ImageFloat>;

/// <summary> Image in RGB colorspace </summary>
using ImageRGB = ImageVec3;
/// <summary> Image in XYZ colorspace </summary>
using ImageXYZ = ImageFloatPlane3;


/// <summary> Gradient of a scalar image </summary>
struct ImageGradient {
    ImageFloat dx;
    ImageFloat dy;
};

/// <summary> Gradient of a XYZ image (contains one dxy-gradient image per channel) </summary>
using ImageXYZGradient = ImagePlane3<ImageGradient>;





/// <summary>
/// Prints helpful information about OpenMP.
/// </summary>
void printOpenMPStatus() 
{
#ifdef _OPENMP
    // https://stackoverflow.com/questions/38281448/how-to-check-the-version-of-openmp-on-windows
    std::cout << "OpenMP version " << _OPENMP << " is ENABLED with " << omp_get_max_threads() << " threads." << std::endl;
#else
    std::cout << "OpenMP is DISABLED." << std::endl;
#endif
}

/// <summary>
/// Converts gradients to RGB for visualization.
/// Red channel = dX, Green  = dY, Blue = 0.
/// </summary>
/// <param name="gradient"></param>
/// <returns></returns>
ImageRGB gradientsToRgb(const ImageGradient& gradient) {
    auto grad_rgb = ImageRGB(gradient.dx.width, gradient.dx.height);
    #pragma omp parallel for
    for (auto i = 0; i < grad_rgb.data.size(); i++) {
        grad_rgb.data[i] = glm::abs(glm::vec3(gradient.dx.data[i], gradient.dy.data[i], 0.0f));
    }
    return grad_rgb;
}


/// <summary>
/// Converts float image to RGB by repeating the channel 3x.
/// </summary>
/// <param name="img"></param>
/// <returns></returns>
ImageRGB imageFloatToRgb(const ImageFloat& img) {
    auto result = ImageRGB(img.width, img.height);
    #pragma omp parallel for
    for (int i = 0; i < result.data.size(); i++) {
        result.data[i] = glm::vec3(img.data[i], img.data[i], img.data[i]);
    }
    return result;
}

/// <summary>
/// Converts RGB image to float by selecting the Red channel.
/// </summary>
/// <param name="img"></param>
/// <returns></returns>
ImageFloat imageRgbToFloat(const ImageRGB& img)
{
    auto result = ImageFloat(img.width, img.height);
#pragma omp parallel for
    for (int i = 0; i < result.data.size(); i++) {
        result.data[i] = img.data[i].x;
    }
    return result;
}



/// <summary>
/// Compute natural log of image.
/// </summary>
/// <param name="image"></param>
/// <returns></returns>
ImageFloat logImage(const ImageFloat& image)
{
    auto result = ImageFloat(image.width, image.height);
#pragma omp parallel for
    for (int i = 0; i < image.data.size(); i++) {
        result.data[i] = logf(std::max(image.data[i], 1e-8f));
    }
    return result;
}


/// <summary>
/// Computes detail layer as a difference in log space.
/// All inputs/outputs are assumed to be in log space.
/// </summary>
/// <param name="H"></param>
/// <param name="base"></param>
/// <returns></returns>
ImageFloat getDetailImage(const ImageFloat& H, const ImageFloat& base)
{
    // Empty output image.
    auto result = ImageFloat(H.width, H.height);
#pragma omp parallel for
    for (int i = 0; i < result.data.size(); i++) {
        result.data[i] = H.data[i] - base.data[i];
    }
    return result;
}

/*

Unused code.

*/


/// <summary>
/// https://en.wikipedia.org/wiki/CIE_1931_color_space
/// </summary>
/// <param name="rgb"></param>
/// <returns></returns>
ImageXYZ rgbToXYZ(const ImageRGB& rgb)
{
    auto xyz = ImageXYZ(ImageFloat(rgb.width, rgb.height), ImageFloat(rgb.width, rgb.height), ImageFloat(rgb.width, rgb.height));

    const auto MAT_RGB_TO_XYZ = glm::transpose(glm::mat3(0.49f, 0.31f, 0.2f, 0.17697f, 0.8124f, 0.01063f, 0.0f, 0.01f, 0.99000f));

#pragma omp parallel for
    for (int i = 0; i < rgb.data.size(); i++) {
        auto v = MAT_RGB_TO_XYZ * rgb.data[i];
        xyz.X.data[i] = v.x;
        xyz.Y.data[i] = v.y;
        xyz.Z.data[i] = v.z;
    }

    return xyz;
}

/// <summary>
/// https://en.wikipedia.org/wiki/CIE_1931_color_space
/// </summary>
/// <param name="rgb"></param>
/// <returns></returns>
ImageRGB xyzToRGB(const ImageXYZ& xyz)
{
    auto rgb = ImageRGB(xyz.X.width, xyz.X.height);

    const auto MAT_RGB_TO_XYZ = glm::transpose(glm::mat3(0.49f, 0.31f, 0.2f, 0.17697f, 0.8124f, 0.01063f, 0.0f, 0.01f, 0.99000f));
    const auto MAT_XYZ_TO_RGB = glm::inverse(MAT_RGB_TO_XYZ);

#pragma omp parallel for
    for (int i = 0; i < xyz.X.data.size(); i++) {
        auto v = glm::vec3(xyz.X.data[i], xyz.Y.data[i], xyz.Z.data[i]);
        auto xyz = MAT_XYZ_TO_RGB * v;
        rgb.data[i] = xyz;
    }

    return rgb;
}


/// <summary>
/// Switches from Attribute layout to Plane layout.
/// </summary>
/// <param name="image">image in attribute order</param>
/// <returns>image in plane order</returns>
ImageFloatPlane3 imageVec3ToPlane3(const ImageVec3& image)
{
    auto result = ImageFloatPlane3({ image.width, image.height }, { image.width, image.height }, { image.width, image.height });
#pragma omp parallel for
    for (int i = 0; i < image.data.size(); i++) {
        for (auto j = 0; j < 3; j++) {
            result[j].data[i] = image.data[i][j];
        }
    }
    return result;
}

/// <summary>
/// Switches from Attribute layout to Plane layout.
/// </summary>
/// <param name="image">image in attribute order</param>
/// <returns>image in plane order</returns>
ImageVec3 imagePlane3ToVec3(const ImageFloatPlane3& image)
{
    auto result = ImageVec3(image.X.width, image.X.height);
#pragma omp parallel for
    for (int i = 0; i < image.X.data.size(); i++) {
        for (auto j = 0; j < 3; j++) {
            result.data[i][j] = image[j].data[i];
        }
    }
    return result;
}


