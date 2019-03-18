/**
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 */

#ifndef __DSOPENALPR_LIB__
#define __DSOPENALPR_LIB__

#include <alprstream.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DsOpenalprCtx DsOpenalprCtx;

// This value should be greater than the number of frames that DeepStream
// could possibly send OpenALPR before triggering a processing event.
// If it's too large, it will use more memory, but otherwise won't affect efficiency
// if the value is too small, and DeepStream pushes a frame while we're full, a random frame is dropped from the queue
// When processing happens, the queue is cleared.
const int MAX_FRAME_QUEUE_SIZE = 30;

typedef struct
{
  /// Width of the images being provided by DeepStream
  int processing_width;
  
  /// height of the images being provided by DeepStream
  int processing_height;

  // Number of channels in Mat provided by DeepStream
  int processing_channels;
  
  /// Country controls the OpenALPR database loaded.  For example, USA/Mexico/Canada would be "us"
  /// European plates would be "eu", Australia "au", etc.  A full list can be found in /usr/share/openalpr/runtime_data/config/
  const char* country;
  
  /// The full path to the OpenALPR config file.  Default is /etc/openalpr/openalpr.conf if left blank.
  const char* config_file;
  
  /// The full path to the OpenALPR runtime_data dir.  Default is /usr/share/openalpr/runtime_data if left blank.
  const char* runtime_dir;
  
  /// The commercial license key to run.  If left blank it will use the value in /etc/openalpr/license.conf
  const char* license_key;
  
  /// Perform vehicle recognition (once per vehicle not once per plates).  Can be disabled if not used to save memory
  /// Requires group_plates_across_frames to be enabled in order to work
  int recognize_vehicles;
  
  /// Group plates across frames in addition to returning individual results per frame.  Can be disabled to save memory.
  int group_plates_across_frames;
} DsOpenalprInitParams;


typedef struct
{
  /// Individual frame result (one for each image frame processed)
  std::vector<alpr::AlprResults> frame_results;
  
  /// Vehicle results are sent after the plate is no longer seen
  /// one per vehicle (delayed), unrelated to the number of images
  std::vector<alpr::AlprGroupResult> group_results;

} DsOpenalprOutput;

// Initialize library context
DsOpenalprCtx * DsOpenalprCtx_Init (DsOpenalprInitParams *init_params);

// Queue input for processing
void DsOpenalpr_QueueInput (DsOpenalprCtx *ctx, unsigned char *data);

// Dequeue processed output
DsOpenalprOutput *DsOpenalpr_DequeueOutput (DsOpenalprCtx *ctx);

// Deinitialize library context
void DsOpenalprCtx_Deinit (DsOpenalprCtx *ctx);

/// Free the memory used by the output object
void DsOpenalprCtx_ReleaseOutput (DsOpenalprOutput *ds_openalpr_output);

#ifdef __cplusplus
}
#endif

#endif
