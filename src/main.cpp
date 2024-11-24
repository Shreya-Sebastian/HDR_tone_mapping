#include "your_code_here.h"

static const std::filesystem::path dataDirPath { DATA_DIR };
static const std::filesystem::path outDirPath { OUTPUT_DIR };


/// <summary>
/// A Helper function that saves each of the XYZ channel gradients into an RGB image (R = dx, G = dy, B = 0).
/// </summary>
/// <param name="gradients"></param>
/// <param name="name"></param>
void saveGradients(const ImageXYZGradient& gradients, const std::string& name)
{
    for (auto i = 0; i < 3; ++i) {
        gradientsToRgb(gradients[i]).writeToFile(outDirPath / (name + "_" + "XYZ"[i] + ".png"));
    }
}


/// <summary>
/// Main method. Runs default tests. Feel free to modify it, add more tests and experiments,
/// change the input images etc. The file is not part of the solution. All solutions have to 
/// implemented in "your_code_here.h".
/// </summary>
/// <returns>0</returns>
int main()
{
    std::chrono::steady_clock::time_point time_start, time_end;
    printOpenMPStatus();

    #pragma region HDR TMO
    //////////////////////////////////////////////////////////////////////////////
    /// Part I: HDR Tone Mapping
    //////////////////////////////////////////////////////////////////////////////

    // 0. Load inputs from files. https://www.cs.huji.ac.il/~danix/hdr/pages/memorial.html
    auto hdr_image = ImageRGB(dataDirPath / "memorial2_half.hdr");
    hdr_image.writeToFile(outDirPath / "0_src.png");

    // 1. Normalize the image range to [0,1].
    auto image_normed = normalizeRGBImage(hdr_image);
    image_normed.writeToFile(outDirPath / "1_normalized.png");

    // 2. Apply gamma curve.
    auto image_gamma = applyGamma(image_normed, 1 / 2.2f);
    image_gamma.writeToFile(outDirPath / "2_gamma.png");

    // 2b. Apply gamma to the original image.
    auto gamma_orig = applyGamma(hdr_image, 1 / 2.2f);
    gamma_orig.writeToFile(outDirPath / "2_gamma_orig.png");

    // 3. Get luminance.
    auto hdr_luminance = rgbToLuminance(hdr_image);
    hdr_luminance.writeToFile(outDirPath / "3a_luminance.png");
    // [Provided] Compute Logarithm of the luminance
    auto log_lum_H = logImage(hdr_luminance);
    normalizeFloatImage(log_lum_H).writeToFile(outDirPath / "3b_log_luminance_H.png");

    // 4. Apply bilateral filter.
    const int filter_size = 27; // must be an odd integer
    const float space_sigma = filter_size / 6.4f;
    const float range_sigma = 1.0f;
    auto base_image = bilateralFilter(log_lum_H, filter_size, space_sigma, range_sigma);
    normalizeFloatImage(base_image).writeToFile(outDirPath / "4_base_layer.png");

    // [Provided] Get Detail image.
    auto detail_image = getDetailImage(log_lum_H, base_image);
    normalizeFloatImage(detail_image).writeToFile(outDirPath / "5_detail_layer.png");

    // 6. Get new intensity after contrast reduction.
    const float base_scale = 0.15f;
    const float output_gain = 0.5f;
    auto tmo_luminance = applyDurandToneMappingOperator(base_image, detail_image, base_scale, output_gain);
    tmo_luminance.writeToFile(outDirPath / "6_tmo_luminance.png");

    // 7. Convert back to RGB.
    auto tmo_rgb = rescaleRgbByLuminance(hdr_image, hdr_luminance, tmo_luminance);
    tmo_rgb.writeToFile(outDirPath / "7_tmo_rgb.png");

    #pragma endregion HDR TMO

    #pragma region Poisson
    //////////////////////////////////////////////////////////////////////////////
    /// Part II: Poisson Image Editing
    //////////////////////////////////////////////////////////////////////////////

    // [Provided]  Read Mask and source images
    auto target_image = tmo_rgb;
    auto source_image = ImageRGB(dataDirPath / "cat.png");
    auto source_mask = imageRgbToFloat(ImageRGB(dataDirPath / "cat_mask.png"));

    // [Optional] Alternative test inputs (make your own!)
    /*auto target_image = ImageRGB(dataDirPath / "plane_target.jpg");
    auto source_image = ImageRGB(dataDirPath / "plane_src.jpg");
    auto source_mask = imageRgbToFloat(ImageRGB(dataDirPath / "plane_mask.png"));*/

    // [Provided]  Convert colorspace RGB->XYZ
    auto target_image_XYZ = rgbToXYZ(target_image);
    auto source_image_XYZ = rgbToXYZ(source_image);
    //auto target_image_XYZ = imageVec3ToPlane3(target_image); // use this to by-pass the RGB->XYZ conversion and calculate in RGB space. The final results might often be similar.
    //auto source_image_XYZ = imageVec3ToPlane3(source_image);
    imagePlane3ToVec3(target_image_XYZ).writeToFile(outDirPath / "7b_target_xyz.png");
    imagePlane3ToVec3(source_image_XYZ).writeToFile(outDirPath / "7c_source_xyz.png");

    // 8.  Compute gradients of source.
    auto source_gradients_XYZ = getGradientsXYZ(source_image_XYZ);
    saveGradients(source_gradients_XYZ, "8a_source_gradients");

    // 8.  Compute gradients of target.
    auto target_gradients_XYZ = getGradientsXYZ(target_image_XYZ);
    saveGradients(target_gradients_XYZ, "8b_target_gradients");

    // 9.  Merge the two gradient images following the mask.
    auto merged_gradients_XYZ = copySourceGradientsToTargetXYZ(source_gradients_XYZ, target_gradients_XYZ, source_mask);
    saveGradients(merged_gradients_XYZ, "9_merged_gradients");
    //merged_gradients_XYZ = target_gradients_XYZ;

    // 9.  Compute the divergence.
    auto divergence_XYZ = getDivergenceXYZ(merged_gradients_XYZ);
    normalizeRGBImage(imagePlane3ToVec3(divergence_XYZ)).writeToFile(outDirPath / "10_divergence.png");
    
    // 11. Solve Poisson equations per channel (XYZ)
    auto edit_result_XYZ = solvePoissonXYZ(target_image_XYZ, divergence_XYZ, 2000);
    imagePlane3ToVec3(edit_result_XYZ).writeToFile(outDirPath / "11_edit_result_XYZ.png");

    // [Provided] 12. XYZ to RGB
    auto edit_result_rgb = xyzToRGB(edit_result_XYZ);
    edit_result_rgb.writeToFile(outDirPath / "12_edit_result_rgb.png");


    #pragma endregion Poisson

    std::cout << "All done!" << std::endl;
    return 0;
}


