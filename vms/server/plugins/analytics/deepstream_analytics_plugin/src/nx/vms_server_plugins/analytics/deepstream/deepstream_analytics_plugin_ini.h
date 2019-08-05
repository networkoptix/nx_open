#pragma once

#include <nx/kit/ini_config.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

static const int kDefaultPipeline = 1;
static const int kOpenAlprPipeline = 2;

static const int kDeepStreamDefaultMetadtaDurationMs = 30;
static const int kOpenAlprDefaultMetadtaDurationMs = 100;

struct DeepStreamConfig: public nx::kit::IniConfig
{
    DeepStreamConfig():
        nx::kit::IniConfig("deepstream_analytics_plugin.ini")
    {
        reload();
    }

//------------------------------------------------------------------------------------------------
// Common settings.

    NX_INI_INT(
        kDefaultPipeline,
        //kDefaultPipeline,
        pipelineType,
        "Type of pipeline: 1 - default pipeline, 2 - OpenALPR pipeline");

    NX_INI_INT(
        kDeepStreamDefaultMetadtaDurationMs,
        deepstreamDefaultMetadataDurationMs,
        "Default duration assigned to output metadata packets if pipelineType is 1");

    NX_INI_INT(
        kOpenAlprDefaultMetadtaDurationMs,
        openAlprDefaultMetadataDurationMs,
        "Default duration assigned to output metadata packets if pipelineType is 2");

    NX_INI_FLAG(1, showGuids, "Show object guids");

    NX_INI_STRING("", debugDotFilesDir, "Directory for pipeline graph .dot files");
    NX_INI_INT(0, gstreamerDebugLevel, "Verbosity of GStreamer library, Range is: [0, 8]");

    NX_INI_FLAG(1, enableOutput, "Enable logging");
    NX_INI_INT(
        1920,
        defaultFrameWidth,
        "Frame width to use in the case if incoming data has no information about resolution");

    NX_INI_INT(
        1080,
        defaultFrameHeight,
        "Frame height to use in the case if incoming data has no information about resolution");

//------------------------------------------------------------------------------------------------
// OpenALPR settings.

    NX_INI_FLAG(
        0,
        enableOpenAlpr,
        "Enable OpenALPR plugin");

    NX_INI_STRING(
        "full-frame",
        openAlprMode,
        "OpenALPR recognition mode. Possible values: \"full-frame\" - operate on full frames, "
        "\"crop\" - operate on primary GIE crops.");

    NX_INI_INT(
        500,
        maxAllowedFrameDelayMs,
        "Maximum allowed frame delay.");

    NX_INI_INT(
        3000,
        licensePlateLifetimeMs,
        "License plate preserves the same GUID during this period even if not appearing on the\n"
        "scene.");

    NX_INI_FLAG(
        0,
        generateEventWhenLicensePlateDisappears,
        "Generate an event when license plate has not been detected for more than\n"
        "licensePlateLifetimeMs");

//------------------------------------------------------------------------------------------------
// Primary GIE settings.

    NX_INI_STRING(
        "/home/nvidia/Model/ResNet_18/class_guids.txt",
        pgie_classGuidsFile,
        "File containing guids for classes that primary GIE can detect.");

    NX_INI_FLAG(1, pgie_enabled, "Whether primary GIE is enabled or not");
    NX_INI_INT(0, pgie_colorFormat, "Color format. 0 = RGBA, 1 = BGRA");
    NX_INI_STRING(
        "0;0;0",
        pgie_colorOffsets,
        "Array of mean values of color components to be subtracted from each pixel");

    NX_INI_INT(3, pgie_numberOfClasses, "Number of classes the model can detect");
    NX_INI_STRING(
        "0,0.2,0.1,3,0:1,0.2,0.1,3,0:2,0.2,0.1,3,0:",
        pgie_classThresholds,
        "Thresholding Parameters for all classes. Specified per-class. "
        "Format: CLASS_ID_0(int),CONFIDENCE_THRESHOLD_0(float),EPSILON_0(float), "
        "GROUP_THRESHOLD_0(int),MIN_BOXES_0(int):CLASS_ID_0...");

    NX_INI_FLOAT(0.0039215697906911373f, pgie_netScaleFactor, "Pixel normalization factor");
    NX_INI_INT(16, pgie_netStride, "Convolutional neural network stride");

    NX_INI_STRING(
        "/home/nvidia/Model/ResNet_18/ResNet_18_threeClass_VGA_pruned.caffemodel",
        pgie_modelFile,
        "Absolute path to model file");

    NX_INI_STRING(
        "/home/nvidia/Model/ResNet_18/ResNet_18_threeClass_VGA_deploy_pruned.prototxt",
        pgie_protoFile,
        "Absolute path to proto file");

    NX_INI_STRING(
        "/home/nvidia/Model/ResNet_18/ResNet_18_threeClass_VGA_pruned.caffemodel_b2_fp16.cache",
        pgie_cacheFile,
        "Absolute path to cache file");

    NX_INI_STRING(
        "",
        pgie_meanFile,
        "Absolute path to mean file");

    NX_INI_STRING(
        "/home/nvidia/Model/ResNet_18/labels.txt",
        pgie_labelFile,
        "Absolute path to label file");

    NX_INI_STRING(
        "Layer11_bbox",
        pgie_outputBboxLayerName,
        "Name of the Neural Network layer which outputs bounding box coordinates");

    NX_INI_STRING(
        "Layer11_cov",
        pgie_outputCoverageLayerNames,
        "Array of the coverage layer names. Array should be semicolon seperated. "
        "Format: coverage_layer0;coverage_layer1;coverage_layer2");

    NX_INI_INT(
        2,
        pgie_batchSize,
        "Number of units [frames(PGIE) or objects(SGIE)] to be inferred together in a batch");

    NX_INI_INT(
        20,
        pgie_inferenceInterval,
        "Specifies number of consecutive frames to be skipped for inference. "
        "Actual frames to be skipped = batch_size * interval");

    NX_INI_STRING(
        "0,0,0:1,0,0:2,0,0:",
        pgie_minDetectedWH,
        "Minimum size in pixels of detected objects that will be outputted by the GIE. "
        "Specified per-class. Format: class-id,min-w,min-h:class-id,min-w,min-h:...");

    NX_INI_STRING(
        "0,1920,1080:1,1920,1080:2,1920,1080:",
        pgie_maxDetectedWH,
        "Maximum size in pixels of detected objects that will be outputted by the GIE. "
        "Specified per-class. Format: class-id,max-w,max-h:class-id,max-w,max-h:...");

    NX_INI_STRING(
        "0,0:1,0:2,0:",
        pgie_roiTopOffset,
        "Offset of the ROI from the top of the frame. "
        "Only objects within the ROI will be outputted. "
        "Format:  class-id,top-offset:class-id,top-offset:...");

    NX_INI_STRING(
        "0,0:1,0:2,0:",
        pgie_roiBottomOffset,
        "Offset of the ROI from the bottom of the frame. "
        "Only objects within the ROI will be outputted. "
        "Format:  class-id,bottom-offset:class-id,bottom-offset:...");

    NX_INI_FLAG(
        0,
        pgie_forceFp32,
        "When enabled forces use of fp32. When disabled uses fp16 if platform supports it");

    NX_INI_INT(
        1,
        pgie_gieMode,
        "GIE Mode (1=Primary Mode 2=Secondary Mode)");

    NX_INI_INT(
        1,
        pgie_gieUniqueId,
        "Unique ID used to identify metadata generated by this GIE");

    NX_INI_INT(
        0,
        pgie_makeInferenceOnGieWithId,
        "Infer on metadata generated by GIE with this unique ID");

    NX_INI_STRING(
        "0:1:2",
        pgie_makeInferenceOnClassId,
        "Infer on objects with specified class ids");

    NX_INI_FLAG(
        0,
        pgie_isClassifier,
        "Whether this GIE is a classifier");

    NX_INI_FLOAT(
        0.65f,
        pgie_secondaryGieClassifierThreshold,
        "Threshold for classifier. Only when this GIE is a secondary classifier");

    NX_INI_FLAG(
        0,
        pgie_enableDbScan,
        "When enabled uses dbscan. When disabled uses grouprectangles");

//------------------------------------------------------------------------------------------------
// Secondary GIE 0 settings.

    NX_INI_FLAG(1, sgie0_enabled, "Whether secondary GIE0 is enabled or not");
    NX_INI_INT(0, sgie0_colorFormat, "Color format. 0 = RGBA, 1 = BGRA");
    NX_INI_STRING(
        "73.00;77.55;88.9",
        sgie0_colorOffsets,
        "Array of mean values of color components to be subtracted from each pixel");

    NX_INI_INT(1, sgie0_numberOfClasses, "Number of classes the model can detect");
    NX_INI_STRING(
        "",
        sgie0_classThresholds,
        "Thresholding Parameters for all classes. Specified per-class. "
        "Format: CLASS_ID_0(int),CONFIDENCE_THRESHOLD_0(float),EPSILON_0(float),"
        "GROUP_THRESHOLD_0(int),MIN_BOXES_0(int):CLASS_ID_0...");

    NX_INI_FLOAT(1.0f, sgie0_netScaleFactor, "Pixel normalization factor");
    NX_INI_INT(16, sgie0_netStride, "Convolutional neural network stride");

    NX_INI_STRING(
        "/home/nvidia/Model/ivaSecondary_VehicleTypes_V1/snapshot_iter_13740.caffemodel",
        sgie0_modelFile,
        "Absolute path to model file");

    NX_INI_STRING(
        "/home/nvidia/Model/ivaSecondary_VehicleTypes_V1/deploy.prototxt",
        sgie0_protoFile,
        "Absolute path to proto file");

    NX_INI_STRING(
        "/home/nvidia/Model/ivaSecondary_VehicleTypes_V1"
        "/snapshot_iter_13740.caffemodel_b2_fp16.cache",
        sgie0_cacheFile,
        "Absolute path to cache file");

    NX_INI_STRING("", sgie0_meanFile, "Absolute path to mean file");
    NX_INI_STRING(
        "/home/nvidia/Model/ivaSecondary_VehicleTypes_V1/labels.txt",
        sgie0_labelFile,
        "Absolute path to label file");
    NX_INI_STRING(
        "Vehicle type",
        sgie0_labelType,
        "Label type for SGIE0");

    NX_INI_STRING(
        "",
        sgie0_outputBboxLayerName,
        "Name of the Neural Network layer which outputs bounding box coordinates");

    NX_INI_STRING(
        "softmax",
        sgie0_outputCoverageLayerNames,
        "Array of the coverage layer names. Array should be semicolon seperated. "
        "Format: coverage_layer0;coverage_layer1;coverage_layer2");

    NX_INI_INT(
        2,
        sgie0_batchSize,
        "Number of units [frames(PGIE) or objects(SGIE)] to be inferred together in a batch");

    NX_INI_INT(
        0,
        sgie0_inferenceInterval,
        "Specifies number of consecutive frames to be skipped for inference. "
        "Actual frames to be skipped = batch_size * interval");

    NX_INI_STRING(
        "0,128,128:1,0,0:2,0,0:3,0,0:4,0,0:5,0,0:",
        sgie0_minDetectedWH,
        "Minimum size in pixels of detected objects that will be outputted by the GIE. "
        "Specified per-class. Format: class-id,min-w,min-h:class-id,min-w,min-h:...");

    NX_INI_STRING(
        "0,1920,1080:1,100,1080:2,1920,1080:3,1920,1080:4,0,0:5,0,0:",
        sgie0_maxDetectedWH,
        "Maximum size in pixels of detected objects that will be outputted by the GIE. "
        "Specified per-class. Format: class-id,max-w,max-h:class-id,max-w,max-h:...");

    NX_INI_STRING(
        "0,0:",
        sgie0_roiTopOffset,
        "Offset of the ROI from the top of the frame. "
        "Only objects within the ROI will be outputted. "
        "Format:  class-id,top-offset:class-id,top-offset:...");

    NX_INI_STRING(
        "0,0:",
        sgie0_roiBottomOffset,
        "Offset of the ROI from the bottom of the frame. "
        "Only objects within the ROI will be outputted. "
        "Format:  class-id,bottom-offset:class-id,bottom-offset:...");

    NX_INI_FLAG(
        0,
        sgie0_forceFp32,
        "When enabled forces use of fp32. When disabled uses fp16 if platform supports it");

    NX_INI_INT(
        2,
        sgie0_gieMode,
        "GIE Mode (1=Primary Mode 2=Secondary Mode)");

    NX_INI_INT(
        2,
        sgie0_gieUniqueId,
        "Unique ID used to identify metadata generated by this GIE");

    NX_INI_INT(
        1,
        sgie0_makeInferenceOnGieWithId,
        "Infer on metadata generated by GIE with this unique ID");

    NX_INI_STRING(
        "2:",
        sgie0_makeInferenceOnClassIds,
        "Infer on objects with specified class ids");

    NX_INI_FLAG(
        1,
        sgie0_isClassifier,
        "Whether this GIE is a classifier");

    NX_INI_FLOAT(
        0.80f,
        sgie0_secondaryGieClassifierThreshold,
        "Threshold for classifier. Only when this GIE is a secondary classifier");

    NX_INI_FLAG(
        0,
        sgie0_enableDbScan,
        "When enabled uses dbscan. When disabled uses grouprectangles");

//------------------------------------------------------------------------------------------------
// Secondary GIE 1 settings.

    NX_INI_FLAG(1, sgie1_enabled, "Whether secondary GIE1 is enabled or not");
    NX_INI_INT(0, sgie1_colorFormat, "Color format. 0 = RGBA, 1 = BGRA");
    NX_INI_STRING(
        "73.00;77.55;88.9",
        sgie1_colorOffsets,
        "Array of mean values of color components to be subtracted from each pixel");

    NX_INI_INT(1, sgie1_numberOfClasses, "Number of classes the model can detect");
    NX_INI_STRING(
        "",
        sgie1_classThresholds,
        "Thresholding Parameters for all classes. Specified per-class. "
        "Format: CLASS_ID_0(int),CONFIDENCE_THRESHOLD_0(float),EPSILON_0(float),"
        "GROUP_THRESHOLD_0(int),MIN_BOXES_0(int):CLASS_ID_0...");

    NX_INI_FLOAT(1.0f, sgie1_netScaleFactor, "Pixel normalization factor");
    NX_INI_INT(16, sgie1_netStride, "Convolutional neural network stride");

    NX_INI_STRING(
        "/home/nvidia/Model/IVASecondary_Make_V1/snapshot_iter_6240.caffemodel",
        sgie1_modelFile,
        "Absolute path to model file");

    NX_INI_STRING(
        "/home/nvidia/Model/IVASecondary_Make_V1/deploy.prototxt",
        sgie1_protoFile,
        "Absolute path to proto file");

    NX_INI_STRING(
        "/home/nvidia/Model/IVASecondary_Make_V1"
        "/snapshot_iter_6240.caffemodel_b2_fp16.cache",
        sgie1_cacheFile,
        "Absolute path to cache file");

    NX_INI_STRING(
        "/home/nvidia/Model/IVASecondary_Make_V1/mean.ppm",
        sgie1_meanFile,
        "Absolute path to mean file");

    NX_INI_STRING(
        "/home/nvidia/Model/IVASecondary_Make_V1/labels.txt",
        sgie1_labelFile,
        "Absolute path to label file");
    NX_INI_STRING(
        "Vendor",
        sgie1_labelType,
        "Label type for SGIE1");

    NX_INI_STRING(
        "",
        sgie1_outputBboxLayerName,
        "Name of the Neural Network layer which outputs bounding box coordinates");

    NX_INI_STRING(
        "softmax",
        sgie1_outputCoverageLayerNames,
        "Array of the coverage layer names. Array should be semicolon seperated. "
        "Format: coverage_layer0;coverage_layer1;coverage_layer2");

    NX_INI_INT(
        2,
        sgie1_batchSize,
        "Number of units [frames(PGIE) or objects(SGIE)] to be inferred together in a batch");

    NX_INI_INT(
        0,
        sgie1_inferenceInterval,
        "Specifies number of consecutive frames to be skipped for inference. "
        "Actual frames to be skipped = batch_size * interval");

    NX_INI_STRING(
        "0,128,128:1,0,0:2,0,0:3,0,0:4,0,0:5,0,0:6,0,0:7,0,0:8,0,0:9,0,0:10,0,0:11,0,0:12,0,0:"
        "13,0,0:14,0,0:15,0,0:16,128,1920:17,0,100:18,0,1920:19,0,1920:20,0,0:21,0,0:"
        "22,0,0:23,0,0",
        sgie1_minDetectedWH,
        "Minimum size in pixels of detected objects that will be outputted by the GIE. "
        "Specified per-class. Format: class-id,min-w,min-h:class-id,min-w,min-h:...");

    NX_INI_STRING(
        "0,1920,1080:1,100,1080:2,1920,1080:3,1920,1080:4,0,0:5,0,0:6,0,0:7,0,0:8,0,0:9,0,0:"
        "10,0,0:11,0,0:12,0,0:13,0,0:14,0,0:15,0,0:16,1080,1:17,1080,1:18,1080,4:19,1080,4:"
        "20,0,0:21,0,0:22,0,0:23,0,0:",
        sgie1_maxDetectedWH,
        "Maximum size in pixels of detected objects that will be outputted by the GIE. "
        "Specified per-class. Format: class-id,max-w,max-h:class-id,max-w,max-h:...");

    NX_INI_STRING(
        "0,0:",
        sgie1_roiTopOffset,
        "Offset of the ROI from the top of the frame. "
        "Only objects within the ROI will be outputted. "
        "Format:  class-id,top-offset:class-id,top-offset:...");

    NX_INI_STRING(
        "0,0:",
        sgie1_roiBottomOffset,
        "Offset of the ROI from the bottom of the frame. "
        "Only objects within the ROI will be outputted. "
        "Format:  class-id,bottom-offset:class-id,bottom-offset:...");

    NX_INI_FLAG(
        0,
        sgie1_forceFp32,
        "When enabled forces use of fp32. When disabled uses fp16 if platform supports it");

    NX_INI_INT(
        2,
        sgie1_gieMode,
        "GIE Mode (1=Primary Mode 2=Secondary Mode)");

    NX_INI_INT(
        3,
        sgie1_gieUniqueId,
        "Unique ID used to identify metadata generated by this GIE");

    NX_INI_INT(
        1,
        sgie1_makeInferenceOnGieWithId,
        "Infer on metadata generated by GIE with this unique ID");

    NX_INI_STRING(
        "2:",
        sgie1_makeInferenceOnClassIds,
        "Infer on objects with specified class ids");

    NX_INI_FLAG(
        1,
        sgie1_isClassifier,
        "Whether this GIE is a classifier");

    NX_INI_FLOAT(
        0.51f,
        sgie1_secondaryGieClassifierThreshold,
        "Threshold for classifier. Only when this GIE is a secondary classifier");

    NX_INI_FLAG(
        0,
        sgie1_enableDbScan,
        "When enabled uses dbscan. When disabled uses grouprectangles");

//------------------------------------------------------------------------------------------------
// Secondary GIE 2 settings.

    NX_INI_FLAG(1, sgie2_enabled, "Whether secondary GIE2 is enabled or not");
    NX_INI_INT(0, sgie2_colorFormat, "Color format. 0 = RGBA, 1 = BGRA");
    NX_INI_STRING(
        "73.00;77.55;88.9",
        sgie2_colorOffsets,
        "Array of mean values of color components to be subtracted from each pixel");

    NX_INI_INT(1, sgie2_numberOfClasses, "Number of classes the model can detect");
    NX_INI_STRING(
        "",
        sgie2_classThresholds,
        "Thresholding Parameters for all classes. Specified per-class. "
        "Format: CLASS_ID_0(int),CONFIDENCE_THRESHOLD_0(float),EPSILON_0(float),"
        "GROUP_THRESHOLD_0(int),MIN_BOXES_0(int):CLASS_ID_0...");

    NX_INI_FLOAT(1.0f, sgie2_netScaleFactor, "Pixel normalization factor");
    NX_INI_INT(16, sgie2_netStride, "Convolutional neural network stride");

    NX_INI_STRING(
        "/home/nvidia/Model/IVA_secondary_carcolor_V1/CarColorPruned.caffemodel",
        sgie2_modelFile,
        "Absolute path to model file");

    NX_INI_STRING(
        "/home/nvidia/Model/IVA_secondary_carcolor_V1/deploy.prototxt",
        sgie2_protoFile,
        "Absolute path to proto file");

    NX_INI_STRING(
        "/home/nvidia/Model/IVA_secondary_carcolor_V1/CarColorPruned.caffemodel_b2_fp16.cache",
        sgie2_cacheFile,
        "Absolute path to cache file");

    NX_INI_STRING(
        "/home/nvidia/Model/IVA_secondary_carcolor_V1/mean.ppm",
        sgie2_meanFile,
        "Absolute path to mean file");

    NX_INI_STRING(
        "/home/nvidia/Model/IVA_secondary_carcolor_V1/labels.txt",
        sgie2_labelFile,
        "Absolute path to label file");
    NX_INI_STRING(
        "Color",
        sgie2_labelType,
        "Label type for SGIE2");

    NX_INI_STRING(
        "",
        sgie2_outputBboxLayerName,
        "Name of the Neural Network layer which outputs bounding box coordinates");

    NX_INI_STRING(
        "softmax",
        sgie2_outputCoverageLayerNames,
        "Array of the coverage layer names. Array should be semicolon seperated. "
        "Format: coverage_layer0;coverage_layer1;coverage_layer2");

    NX_INI_INT(
        2,
        sgie2_batchSize,
        "Number of units [frames(PGIE) or objects(SGIE)] to be inferred together in a batch");

    NX_INI_INT(
        0,
        sgie2_inferenceInterval,
        "Specifies number of consecutive frames to be skipped for inference. "
        "Actual frames to be skipped = batch_size * interval");

    NX_INI_STRING(
        "0,128,128:1,0,0:2,0,0:3,0,0:4,0,0:5,0,0:6,0,0:7,0,0:8,0,0:9,0,0:10,0,0:11,0,0:",
        sgie2_minDetectedWH,
        "Minimum size in pixels of detected objects that will be outputted by the GIE. "
        "Specified per-class. Format: class-id,min-w,min-h:class-id,min-w,min-h:...");

    NX_INI_STRING(
        "0,1920,1080:1,100,1080:2,1920,1080:3,1920,1080:4,0,0:5,0,0:6,0,0:7,0,0:8,0,0:9,0,0:"
        "10,0,0:11,0,0:",
        sgie2_maxDetectedWH,
        "Maximum size in pixels of detected objects that will be outputted by the GIE. "
        "Specified per-class. Format: class-id,max-w,max-h:class-id,max-w,max-h:...");

    NX_INI_STRING(
        "0,0:",
        sgie2_roiTopOffset,
        "Offset of the ROI from the top of the frame. "
        "Only objects within the ROI will be outputted. "
        "Format:  class-id,top-offset:class-id,top-offset:...");

    NX_INI_STRING(
        "0,0:",
        sgie2_roiBottomOffset,
        "Offset of the ROI from the bottom of the frame. "
        "Only objects within the ROI will be outputted. "
        "Format:  class-id,bottom-offset:class-id,bottom-offset:...");

    NX_INI_FLAG(
        0,
        sgie2_forceFp32,
        "When enabled forces use of fp32. When disabled uses fp16 if platform supports it");

    NX_INI_INT(
        2,
        sgie2_gieMode,
        "GIE Mode (1=Primary Mode 2=Secondary Mode)");

    NX_INI_INT(
        4,
        sgie2_gieUniqueId,
        "Unique ID used to identify metadata generated by this GIE");

    NX_INI_INT(
        1,
        sgie2_makeInferenceOnGieWithId,
        "Infer on metadata generated by GIE with this unique ID");

    NX_INI_STRING(
        "2:",
        sgie2_makeInferenceOnClassIds,
        "Infer on objects with specified class ids");

    NX_INI_FLAG(
        1,
        sgie2_isClassifier,
        "Whether this GIE is a classifier");

    NX_INI_FLOAT(
        0.80f,
        sgie2_secondaryGieClassifierThreshold,
        "Threshold for classifier. Only when this GIE is a secondary classifier");

    NX_INI_FLAG(
        0,
        sgie2_enableDbScan,
        "When enabled uses dbscan. When disabled uses grouprectangles");
};

inline DeepStreamConfig& ini()
{
    static DeepStreamConfig ini;
    return ini;
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
