#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <span>
#include <tuple>
#include <vector>

#include "helpers.h"

/*
 * Utility functions.
 */

template<typename T>
int getImageOffset(const Image<T>& image, int x, int y)
{
    // Return offset of the pixel at column x and row y in memory such that 
    // the pixel can be accessed by image.data[offset].
    // 
    // Note, that the image is stored in row-first order, 
    // ie. is the order of [x,y] pixels is [0,0],[1,0],[2,0]...[0,1],[1,1][2,1],...
    //
    // Image size can be accessed using image.width and image.height.
    
    /*******
     * TODO: YOUR CODE GOES HERE!!!
     ******/

    return 0;
}


#pragma region HDR TMO

glm::vec2 getRGBImageMinMax(const ImageRGB& image) {

    auto min_val = 100.0f;
    auto max_val = -1.0f;

    // Write a code that will return minimum value (min of all color channels and pixels) and maximum value as a glm::vec2(min,max).
    
    // Note: Parallelize the code using OpenMP directives for full points.
    
   #pragma omp parallel for
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            glm::vec3 val = image.data[y * image.width + x];

            float max_rgb = std::max(std::max(val.r, val.g), val.b);
            float min_rgb = std::min(std::min(val.r, val.g), val.b);

            if (min_rgb < min_val) {
                min_val = min_rgb;
            }
            if (max_rgb > max_val) {
                max_val = max_rgb;
            }
        }

    }

    // Return min and max value as x and y components of a vector.
    return glm::vec2(min_val, max_val);
}

ImageRGB normalizeRGBImage(const ImageRGB& image)
{
    // Create an empty image of the same size as input.
    auto result = ImageRGB(image.width, image.height);

    // Find min and max values.
    glm::vec2 min_max = getRGBImageMinMax(image);

    // Fill the result with normalized image values (ie, fit the image to [0,1] range).    
    

    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            glm::vec3 val = image.data[y * image.width + x];

            glm::vec3 I_norm = (val - min_max.x) / (min_max.y - min_max.x);

            result.data[y * image.width + x] = I_norm;
        }
    }

    return result;
}

ImageRGB applyGamma(const ImageRGB& image, const float gamma)
{
    // Create an empty image of the same size as input.
    auto result = ImageRGB(image.width, image.height);
    auto Inorm = normalizeRGBImage(image);
    // Fill the result with gamma mapped pixel values (result = image^gamma).    

    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            glm::vec3 val = image.data[y * image.width + x];
            //glm::vec3 val = Inorm.data[y * image.width + x];

            float R = pow(val.r, gamma);
            float G = pow(val.g, gamma);
            float B = pow(val.b, gamma);

            glm::vec3 rgb(R, G, B);

            result.data[y * image.width + x] = rgb;
        }
    }
    

    return result;
}

/*
    Main algorithm.
*/

/// <summary>
/// Compute luminance from a linear RGB image.
/// </summary>
/// <param name="rgb">A linear RGB image</param>
/// <returns>log-luminance</returns>
ImageFloat rgbToLuminance(const ImageRGB& rgb)
{
    // RGB to luminance weights defined in ITU R-REC-BT.601 in the R,G,B order.
    const auto WEIGHTS_RGB_TO_LUM = glm::vec3(0.299f, 0.587f, 0.114f);
    // An empty luminance image.
    auto luminance = ImageFloat(rgb.width, rgb.height);
    // Fill the image by logarithmic luminace.
    // Luminance is a linear combination of the red, green and blue channels using the weights above.

    for (int y = 0; y < rgb.height; y++) {
        for (int x = 0; x < rgb.width; x++) {
            glm::vec3 val = rgb.data[y * rgb.width + x];

            luminance.data[y * rgb.width + x] = WEIGHTS_RGB_TO_LUM[0] * val.r + WEIGHTS_RGB_TO_LUM[1] * val.g + WEIGHTS_RGB_TO_LUM[2] * val.b;
        }
    }
    return luminance;
}

/// <summary>
/// Applies the bilateral filter on the given intensity image.
/// Crop kernel for areas close to boundary, i.e., pixels that fall outside of the input image are skipped
/// and do not influence the image. 
/// If you see a darkening near borders, you likely do this wrong.
/// </summary>
/// <param name="H">The intensity image to be filtered.</param>
/// <param name="size">The kernel size, which is always odd (size == 2 * radius + 1).</param>
/// <param name="space_sigma">spatial sigma value of a gaussian kernel.</param>
/// <param name="range_sigma">intensity sigma value of a gaussian kernel.</param>
/// <returns>ImageFloat, the filtered intensity.</returns>
ImageFloat bilateralFilter(const ImageFloat& H, const int size, const float space_sigma, const float range_sigma)
{
    // The filter size is always odd.
    assert(size % 2 == 1);

    // Kernel radius.
    int radius = size / 2;

    // Precompute spatial Gaussian weights.
    std::vector<std::vector<float>> spatialWeights(size, std::vector<float>(size));
    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            spatialWeights[i + radius][j + radius] = exp(-(i * i + j * j) / (2.0f * space_sigma * space_sigma));
        }
    }

    // Empty output image.
    auto result = ImageFloat(H.width, H.height);

    for (int y = 0; y < H.height; y++) {
        for (int x = 0; x < H.width; x++) {
            float totalWeight = 0.0f;
            float filteredValue = 0.0f;

            float val = H.data[y * H.width + x];

            // Iterate through the kernel.
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;

                    // Skip out-of-bounds pixels.
                    if (nx < 0 || ny < 0 || nx >= H.width || ny >= H.height) {
                        continue;
                    }

                    float n_val = H.data[ny * H.width + nx];

                    // Compute range weight (intensity difference).
                    float rangeWeight = exp(-(val - n_val) * (val - n_val) / (2.0f * range_sigma * range_sigma));

                    // Compute combined weight --> f(x-y)g(I(x)-I(y))
                    float weight = spatialWeights[dy + radius][dx + radius] * rangeWeight;

                    // Accumulate the weighted value and total weight --> *I(y)
                    filteredValue += n_val * weight;
                    // Normalization factor
                    totalWeight += weight;
                }
            }

            // Normalize the result.
            result.data[y * H.width + x] = filteredValue / totalWeight;
        }
    }

    // Return filtered intensity.
    return result;
}


/// <summary>
/// Reduces contrast of an intensity image decomposed in log space (nautral log => ln) and converts it back to the linear space.
/// Follow instructions from the slides for a correct operation order.
/// </summary>
/// <param name="base_layer">base layer in ln space</param>
/// <param name="detail_layer">detail layer in ln space</param>
/// <param name="base_scale">scaling factor for the base layer</param>
/// <param name="output_gain">scaling factor for the linear output</param>
/// <returns></returns>
ImageFloat applyDurandToneMappingOperator(const ImageFloat& base_layer, const ImageFloat& detail_layer, const float base_scale, const float output_gain)
{
    // Empty output image.
    auto result = ImageFloat(base_layer.width, base_layer.height);

    /*******
     * TODO: YOUR CODE GOES HERE!!!
     ******/

    // Return final result as SDR.
    return result;
}

/// <summary>
/// Rescale RGB by luminances ratio and clamp the output to range [0,1].
/// All values are in "linear space" (ie., not in log space).
/// </summary>
/// <param name="original_rgb">Original RGB image</param>
/// <param name="original_luminance">original luminance</param>
/// <param name="new_luminance">new (target) luminance</param>
/// <param name="saturation">saturation correction coefficient</param>
/// <returns>new RGB image</returns>
ImageRGB rescaleRgbByLuminance(const ImageRGB& original_rgb, const ImageFloat& original_luminance, const ImageFloat& new_luminance, const float saturation = 0.5f)
{
    // EPSILON for thresholding the divisior.
    const float EPSILON = 1e-7f;
    // An empty RGB image for the result.
    auto result = ImageRGB(original_rgb.width, original_rgb.height);

    /*******
     * TODO: YOUR CODE GOES HERE!!!
     ******/

    return result;
}



#pragma endregion HDR TMO


#pragma region Poisson editing


/// <summary>
/// Compute dX and dY gradients of an image. 
/// The output is expected to be 1px bigger than the input to contain all "over-the-boundary" gradients.
/// The input image is considered to be padded by zeros.
/// </summary>
/// <param name="image">input scalar image</param>
/// <returns>grad image</returns>
ImageGradient getGradients(const ImageFloat& image)
{
    // An empty gradient pair (dx, dy).
    auto grad = ImageGradient({ image.width + 1, image.height + 1 }, { image.width + 1, image.height + 1 });

    /*******
     * TODO: YOUR CODE GOES HERE!!!
     ******/

    // Example:
    // grad.dx.data[getImageOffset(grad.dx, 0, 0)] = 0.0f; // TODO: Change this!
    // grad.dy.data[getImageOffset(grad.dy, 0, 0)] = 0.0f; // TODO: Change this!

    // Return both gradients in an ImageGradient struct.
    return grad;
}

/// <summary>
/// Merges two gradient images:
/// - Use source gradients where source_mask > 0.5
/// - Use target gradients where source_mask <= 0.5
/// - Set gradients to 0 for gradients crossing the mask boundary (see slides).
/// Warning: dX and dY gradients often do not cross the boundary at the same time.
/// Refer to the slides for details.
/// </summary>
/// <param name="source"></param>
/// <param name="target"></param>
/// <param name="source_mask"></param>
/// <returns></returns>
ImageGradient copySourceGradientsToTarget(const ImageGradient& source, const ImageGradient& target, const ImageFloat& source_mask)
{   
    // An empty gradient pair (dx, dy).
    ImageGradient result = ImageGradient({ target.dx.width, target.dx.height }, { target.dx.width, target.dx.height });

    /*******
     * TODO: YOUR CODE GOES HERE!!!
     ******/

    return result;
}



/// <summary>
/// Computes divergence from gradients.
/// The output is expected to be 1px bigger than the gradients to contain all "over-the-boundary" derivatives.
/// The input gradient image is considered to be padded by zeros.
/// Refer to the slides for details.
/// </summary>
/// <param name="gradients">gradients</param>
/// <returns>div G</returns>
ImageFloat getDivergence(ImageGradient& gradients)
{

    // An empty divergence field.
    auto div_G = ImageFloat(gradients.dx.width + 1, gradients.dx.height + 1);

    /*******
     * TODO: YOUR CODE GOES HERE!!!
     ******/

    return div_G;
}





/// <summary>
/// Solves poisson equation in form grad^2 I = div G.
/// </summary>
/// <param name="initial_solution">initial solution</param>
/// <param name="divergence_G">div G</param>
/// <param name="num_iters">number of iterations</param>
/// <returns>luminance I</returns>
ImageFloat solvePoisson(const ImageFloat& initial_solution, const ImageFloat& divergence_G, const int num_iters = 2000)
{
    // Initial solution guess.
    auto I = ImageFloat(initial_solution);

    // Another solution for the alteranting updates.
    auto I_next = ImageFloat(I.width, I.height);

    // Iterative solver.
    for (auto iter = 0; iter < num_iters; iter++)
    {
        if (iter % 500 == 0) {
            // Print progress info every 500 iteartions.
            std::cout << "[" << iter << "/" << num_iters << "] Solving Poisson equation..." << std::endl;
        }

        // Compute values of I based following the update rule in the slides.

        // Note: Parallelize the code using OpenMP directives for full points.

        /*******
         * TODO: YOUR CODE GOES HERE!!!
         ******/
        // Swaps the current and next solution so that the next iteration
        // uses the new solution as input and the previous solution as output.
        std::swap(I, I_next);
    }

    // After the last "swap", I is the latest solution.
    return I;
}

#pragma endregion Poisson editing

// Below are pre-implemented parts of the code.

#pragma region Functions applying per-channel operation to all planes of an XYZ image

/// <summary>
/// A helper function computing X and Y gradients of an XYZ image by calling getGradient() to each channel.
/// </summary>
/// <param name="image">XYZ image.</param>
/// <returns>Grad per channel.</returns>
ImageXYZGradient getGradientsXYZ(const ImageXYZ& image)
{
    return {
        getGradients(image.X),
        getGradients(image.Y),
        getGradients(image.Z),
    };
}

/// <summary>
/// A helper function computing divergence of an XYZ gradient image by calling getDivergence() to each channel.
/// </summary>
/// <param name="grad_xyz">gradients of XYZ</param>
/// <returns>div G</returns>
ImageXYZ getDivergenceXYZ(ImageXYZGradient& grad_xyz)
{
    return {
        getDivergence(grad_xyz.X),
        getDivergence(grad_xyz.Y),
        getDivergence(grad_xyz.Z),
    };
}

/// <summary>
/// Applies copySourceGradientsToTarget() per channel.
/// </summary>
/// <param name="source">source</param>
/// <param name="target">target</param>
/// <param name="source_mask">target</param>
/// <returns>gradient</returns>
ImageXYZGradient copySourceGradientsToTargetXYZ(const ImageXYZGradient& source, const ImageXYZGradient& target, const ImageFloat& source_mask)
{
    return {
        copySourceGradientsToTarget(source.X, target.X, source_mask),
        copySourceGradientsToTarget(source.Y, target.Y, source_mask),
        copySourceGradientsToTarget(source.Z, target.Z, source_mask),
    };
}

/// <summary>
/// Solves poisson equation in form grad^2 I = div G for each channel.
/// </summary>
/// <param name="divergence_G">div G</param>
/// <param name="num_iters">number of iterations</param>
/// <returns>luminance I</returns>
ImageXYZ solvePoissonXYZ(const ImageXYZ& targetXYZ, const ImageXYZ& divergenceXYZ_G, const int num_iters = 2000)
{
    return {
        solvePoisson(targetXYZ.X, divergenceXYZ_G.X, num_iters),
        solvePoisson(targetXYZ.Y, divergenceXYZ_G.Y, num_iters),
        solvePoisson(targetXYZ.Z, divergenceXYZ_G.Z, num_iters),
    };
}


#pragma endregion

#pragma region Convenience functions

/// <summary>
/// Normalizes single channel image to 0..1 range.
/// </summary>
/// <param name="image"></param>
/// <returns></returns>
ImageFloat normalizeFloatImage(const ImageFloat& image)
{
    return imageRgbToFloat(normalizeRGBImage(imageFloatToRgb(image)));
}



#pragma endregion Convenience functions