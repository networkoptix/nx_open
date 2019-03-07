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

#include "dsopenalpr_lib.h"

#include <glib.h>
#include <stdio.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "openalprimpl.h"

//#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#define DEBUG_PRINTF(f_, ...) 


struct DsOpenalprCtx
{
  DsOpenalprInitParams init_params;
  OpenAlprImpl* impl;
};

DsOpenalprCtx *
DsOpenalprCtx_Init (DsOpenalprInitParams * init_params)
{
  DEBUG_PRINTF("DsOpenalprCtx_Init\n");
  
  DsOpenalprCtx *ctx = (DsOpenalprCtx *) g_malloc (sizeof (DsOpenalprCtx));
  ctx->init_params = *init_params;
  ctx->impl = new OpenAlprImpl(init_params->country, init_params->config_file, 
                               init_params->runtime_dir, init_params->license_key);
  
  if (init_params->group_plates_across_frames)
  {
    if (init_params->recognize_vehicles)
      ctx->impl->enable_vehicle_classifier();
  }
  else
  {
    ctx->impl->disable_grouping();
  }
  
  return ctx;
}

// Feeds input image data into OpenALPR stream buffer
void
DsOpenalpr_QueueInput (DsOpenalprCtx * ctx, unsigned char *data)
{
  DEBUG_PRINTF("DsOpenalpr_QueueInput\n");
  // Queue input. Start processing.
  // Here we just add the input pointer to a queue
  
  ctx->impl->push_cv_mat(data, ctx->init_params.processing_width, 
                         ctx->init_params.processing_height, ctx->init_params.processing_channels);

}

// Processes all frames in buffer.  
// Returns individual results (plate results found in each frame)
// and OpenALPR "group" results (a single plate result per vehicle.
// Group results are delayed until after the vehicle has passed, individual results are instant.
DsOpenalprOutput *
DsOpenalpr_DequeueOutput (DsOpenalprCtx * ctx)
{
  DEBUG_PRINTF("DsOpenalpr_DequeueOutput\n");
  
  // Process the batch
  std::vector<alpr::AlprResults> results = ctx->impl->process_batch();
  DEBUG_PRINTF("DsOpenalpr_DequeueOutput processed batch\n");
  std::vector<alpr::AlprGroupResult> group_results = ctx->impl->pop_groups();
  DEBUG_PRINTF("DsOpenalpr_DequeueOutput popped groups\n");
  
  DsOpenalprOutput *out = new (DsOpenalprOutput);
  
  out->frame_results = results;
  out->group_results = group_results;

  DEBUG_PRINTF("DsOpenalpr_DequeueOutput returning output\n");
  // Processing finished. Dequeue


  return out;
}

void
DsOpenalprCtx_Deinit (DsOpenalprCtx * ctx)
{
  DEBUG_PRINTF("DsOpenalprCtx_Deinit\n");

  delete ctx->impl;
}


void
DsOpenalprCtx_ReleaseOutput (DsOpenalprOutput *ds_openalpr_output)
{
  DEBUG_PRINTF("DsOpenalprCtx_Deinit\n");

  delete ds_openalpr_output;
}
