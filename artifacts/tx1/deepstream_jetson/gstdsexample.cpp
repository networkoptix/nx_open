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

#include <string.h>
#include "gstdsexample.h"

#include <sys/time.h>
#include <iostream>
#include <vector>
#include <deque>

#define USE_EGLIMAGE 1

GST_DEBUG_CATEGORY_STATIC (gst_dsexample_debug);
#define GST_CAT_DEFAULT gst_dsexample_debug

static GQuark _ivameta_quark = 0;

using namespace cv;

struct cached_entry_t
{
  guint tracking_id;
  NvOSD_RectParams rect_params;
  alpr::AlprResults result;
  guint64 frame_num;

  cached_entry_t(const ROIMeta_Params& roi_meta, alpr::AlprResults result_, guint64 frame_num_)
  : tracking_id(roi_meta.tracking_id), rect_params(roi_meta.rect_params)
  , result(result_), frame_num(frame_num_)
  {}
};

class GstDsExample::cached_results_t : public std::deque<cached_entry_t>
{
};

/* Enum to identify properties */
enum
{
  PROP_0,
  PROP_UNIQUE_ID,
  PROP_PROCESSING_WIDTH,
  PROP_PROCESSING_HEIGHT,
  PROP_PROCESS_FULL_FRAME,
};

static const int CACHE_TTL = 10; // Number of frame ticks the entries live in the tracking cache

/* Default values for properties */
#define DEFAULT_UNIQUE_ID 15
#define DEFAULT_PROCESSING_WIDTH 1280
#define DEFAULT_PROCESSING_HEIGHT 720
#define DEFAULT_PROCESS_FULL_FRAME TRUE

/* By default NVIDIA Hardware allocated memory flows through the pipeline. We
 * will be processing on this type of memory only. */
#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"
static GstStaticPadTemplate gst_dsexample_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{ NV12 }")));

static GstStaticPadTemplate gst_dsexample_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{ NV12 }")));

/* Define our element type. Standard GObject/GStreamer boilerplate stuff */
#define gst_dsexample_parent_class parent_class
G_DEFINE_TYPE (GstDsExample, gst_dsexample, GST_TYPE_BASE_TRANSFORM);

static void gst_dsexample_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dsexample_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_dsexample_set_caps (GstBaseTransform * btrans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_dsexample_start (GstBaseTransform * btrans);
static gboolean gst_dsexample_stop (GstBaseTransform * btrans);

static GstFlowReturn gst_dsexample_transform_ip (GstBaseTransform *
    btrans, GstBuffer * inbuf);

static void
attach_metadata_full_frame (GstDsExample * dsexample, GstBuffer * inbuf,
    gdouble scale_ratio, DsOpenalprOutput * output);
static bool attach_metadata_object (GstDsExample * dsexample,
    ROIMeta_Params * roi_meta, DsOpenalprOutput * output);
static bool attach_metadata_object (GstDsExample * dsexample,
    ROIMeta_Params * roi_meta, const alpr::AlprResults* fr=nullptr);

/* Install properties, set sink and src pad capabilities, override the required
 * functions of the base class, These are common to all instances of the
 * element.
 */
static void
gst_dsexample_class_init (GstDsExampleClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  /* Overide base class functions */
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_dsexample_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_dsexample_get_property);

  gstbasetransform_class->set_caps = GST_DEBUG_FUNCPTR (gst_dsexample_set_caps);
  gstbasetransform_class->start = GST_DEBUG_FUNCPTR (gst_dsexample_start);
  gstbasetransform_class->stop = GST_DEBUG_FUNCPTR (gst_dsexample_stop);

  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_dsexample_transform_ip);

  /* Install properties */
  g_object_class_install_property (gobject_class, PROP_UNIQUE_ID,
      g_param_spec_uint ("unique-id", "Unique ID",
          "Unique ID for the element. Can be used to identify output of the element",
          0, G_MAXUINT, DEFAULT_UNIQUE_ID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_PROCESSING_WIDTH,
      g_param_spec_int ("processing-width", "Processing Width",
          "Width of the input buffer to algorithm", 1, G_MAXINT,
          DEFAULT_PROCESSING_WIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_PROCESSING_HEIGHT,
      g_param_spec_int ("processing-height", "Processing Height",
          "Height of the input buffer to algorithm", 1, G_MAXINT,
          DEFAULT_PROCESSING_HEIGHT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_PROCESS_FULL_FRAME,
      g_param_spec_boolean ("full-frame", "Full frame",
          "Enable to process full frame or disable to process objects detected by primary detector",
          DEFAULT_PROCESS_FULL_FRAME,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* Set sink and src pad capabilities */
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_dsexample_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_dsexample_sink_template));

  /* Set metadata describing the element */
  gst_element_class_set_details_simple (gstelement_class,
      "DsExample plugin",
      "DsExample Plugin",
      "Process a 3rdparty example algorithm on objects / full frame",
      "Shaunak Gupte <shaunakg@nvidia.com>");
}

static void
gst_dsexample_init (GstDsExample * dsexample)
{
  GstBaseTransform *btrans = GST_BASE_TRANSFORM (dsexample);

  /* We will not be generating a new buffer. Just adding / updating
   * metadata. */
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (btrans), TRUE);
  /* We do not want to change the input caps. Set to passthrough. transform_ip
   * is still called. */
  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (btrans), TRUE);

  /* Initialize all property variables to default values */
  dsexample->unique_id = DEFAULT_UNIQUE_ID;
  dsexample->processing_width = DEFAULT_PROCESSING_WIDTH;
  dsexample->processing_height = DEFAULT_PROCESSING_HEIGHT;
  dsexample->process_full_frame = DEFAULT_PROCESS_FULL_FRAME;

  /* This quark is required to identify IvaMeta when iterating through
   * the buffer metadatas */
  if (!_ivameta_quark)
    _ivameta_quark = g_quark_from_static_string ("ivameta");
}

/* Function called when a property of the element is set. Standard boilerplate.
 */
static void
gst_dsexample_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (object);
  switch (prop_id) {
    case PROP_UNIQUE_ID:
      dsexample->unique_id = g_value_get_uint (value);
      break;
    case PROP_PROCESSING_WIDTH:
      dsexample->processing_width = g_value_get_int (value);
      break;
    case PROP_PROCESSING_HEIGHT:
      dsexample->processing_height = g_value_get_int (value);
      break;
    case PROP_PROCESS_FULL_FRAME:
      dsexample->process_full_frame = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Function called when a property of the element is requested. Standard
 * boilerplate.
 */
static void
gst_dsexample_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (object);

  switch (prop_id) {
    case PROP_UNIQUE_ID:
      g_value_set_uint (value, dsexample->unique_id);
      break;
    case PROP_PROCESSING_WIDTH:
      g_value_set_int (value, dsexample->processing_width);
      break;
    case PROP_PROCESSING_HEIGHT:
      g_value_set_int (value, dsexample->processing_height);
      break;
    case PROP_PROCESS_FULL_FRAME:
      g_value_set_boolean (value, dsexample->process_full_frame);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * Initialize all resources and start the output thread
 */
static gboolean
gst_dsexample_start (GstBaseTransform * btrans)
{
  const char* OPENALPR_COUNTRY = "us";
  const char* OPENALPR_CONFIG_FILE = "/etc/openalpr/openalpr.conf"; // empty uses default /etc/openalpr/openalpr.conf
  const char* OPENALPR_RUNTIME_DIR = "/usr/share/openalpr/runtime_data/"; // empty uses default /usr/share/openalpr/runtime_data/
  const char* OPENALPR_LICENSE_KEY = ""; // empty reads value from file in /etc/openalpr/license.conf
  int OPENALPR_RECOGNIZE_VEHICLES = 0;
  int OPENALPR_GROUP_PLATES_ACROSS_FRAMES = 1;
  
  GstDsExample *dsexample = GST_DSEXAMPLE (btrans);
  DsOpenalprInitParams init_params =
    { 
      dsexample->processing_width, dsexample->processing_height, 
      3,
      OPENALPR_COUNTRY, OPENALPR_CONFIG_FILE, OPENALPR_RUNTIME_DIR, 
      OPENALPR_LICENSE_KEY,
      OPENALPR_RECOGNIZE_VEHICLES, OPENALPR_GROUP_PLATES_ACROSS_FRAMES
    };

  /* Algorithm specific initializations and resource allocation. */
  dsexample->dsexamplelib_ctx = DsOpenalprCtx_Init (&init_params);

  /* Create a scratch buffer to scale frames or crop and scale objects. In case
   * of full frame processing, even if color conversion and scaling is not
   * required (i.e. frame resolution, color format = processing resolution,
   * color format), this is conversion required since the buffer layout might
   * not be understood by the algorithm. */
  if (NvBufferCreate (&dsexample->conv_dmabuf_fd, dsexample->processing_width,
      dsexample->processing_height, NvBufferLayout_Pitch,
      NvBufferColorFormat_ABGR32) != 0)
    goto error;

  dsexample->cvmat =
      new Mat (Size (dsexample->processing_height,
          dsexample->processing_width), CV_8UC3);
  if (!dsexample->cvmat)
    goto error;


  dsexample->cached_results = new GstDsExample::cached_results_t;

  return TRUE;
error:
  if (dsexample->conv_dmabuf_fd)
    NvBufferDestroy (dsexample->conv_dmabuf_fd);
  if (dsexample->dsexamplelib_ctx)
    DsOpenalprCtx_Deinit (dsexample->dsexamplelib_ctx);
  return FALSE;
}

/**
 * Stop the output thread and free up all the resources
 */
static gboolean
gst_dsexample_stop (GstBaseTransform * btrans)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (btrans);

  NvBufferDestroy (dsexample->conv_dmabuf_fd);

  delete dsexample->cached_results;
  delete dsexample->cvmat;

  // Deinit the algorithm library
  DsOpenalprCtx_Deinit (dsexample->dsexamplelib_ctx);

  return TRUE;
}

/**
 * Called when source / sink pad capabilities have been negotiated.
 */
static gboolean
gst_dsexample_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (btrans);

  /* Save the input video information, since this will be required later. */
  gst_video_info_from_caps (&dsexample->video_info, incaps);

  return TRUE;
}

/**
 * Scale the entire frame to the processing resolution maintaining aspect ratio.
 * Or crop and scale objects to the processing resolution maintaining the aspect
 * ratio. Remove the padding requried by hardware and convert from RGBA to RGB
 * using openCV. These steps can be skipped if the algorithm can work with
 * padded data and/or can work with RGBA.
 */
static GstFlowReturn
get_converted_mat (GstDsExample * dsexample, int in_dmabuf_fd,
    NvOSD_RectParams * crop_rect_params, Mat & out_mat, gdouble & ratio)
{
  NvBufferParams buf_params;
  NvBufferCompositeParams composite_params;
  gpointer mapped_ptr = NULL;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  Mat in_mat;

  // Get buffer parameters of the input buffer
  if (NvBufferGetParams (in_dmabuf_fd, &buf_params) != 0) {
    flow_ret = GST_FLOW_ERROR;
    goto done;
  }

  // Calculate scaling ratio while maintaining aspect ratio
  ratio = MIN (1.0 * dsexample->processing_width / crop_rect_params->width,
      1.0 * dsexample->processing_height / crop_rect_params->height);

  if (ratio < 1.0 / 16 || ratio > 16.0) {
    // Currently cannot scale by ratio > 16 or < 1/16
    flow_ret = GST_FLOW_ERROR;
    goto done;
  }

  memset (&composite_params, 0, sizeof (composite_params));
  // Set black background
  composite_params.composite_bgcolor = (NvBufferCompositeBackground) {
  0, 0, 0};
  // Only one input buffer, set source crop coordinates
  composite_params.input_buf_count = 1;
  composite_params.src_comp_rect[0].left =
      GST_ROUND_UP_2 (crop_rect_params->left);
  composite_params.src_comp_rect[0].top =
      GST_ROUND_UP_2 (crop_rect_params->top);
  composite_params.src_comp_rect[0].width =
      GST_ROUND_DOWN_2 (crop_rect_params->width);
  composite_params.src_comp_rect[0].height =
      GST_ROUND_DOWN_2 (crop_rect_params->height);
  // Place the scaled output in top-left, leaving bottom-right space blank if
  // applicable
  composite_params.dst_comp_rect[0].left = 0;
  composite_params.dst_comp_rect[0].top = 0;
  composite_params.dst_comp_rect[0].width =
      GST_ROUND_DOWN_2 ((gint) (ratio * crop_rect_params->width));
  composite_params.dst_comp_rect[0].height =
      GST_ROUND_DOWN_2 ((gint) (ratio * crop_rect_params->height));
  composite_params.composite_flag = NVBUFFER_COMPOSITE;

  // Actually perform the cropping, scaling
  if (NvBufferComposite (&in_dmabuf_fd, dsexample->conv_dmabuf_fd,
          &composite_params)) {
    flow_ret = GST_FLOW_ERROR;
    goto done;
  }

  // Get the scratch buffer params. We need the pitch of the buffer
  if (NvBufferGetParams (dsexample->conv_dmabuf_fd, &buf_params) != 0) {
    flow_ret = GST_FLOW_ERROR;
    goto done;
  }
  // Map the buffer so that it can be accessed by CPU
  if (NvBufferMemMap (dsexample->conv_dmabuf_fd, 0, NvBufferMem_Read, &mapped_ptr) != 0) {
    flow_ret = GST_FLOW_ERROR;
    goto done;
  }
  // Use openCV to remove padding and convert RGBA to RGB. Can be skipped if
  // algorithm can handle padded RGBA data.
  in_mat =
      Mat (dsexample->processing_height, dsexample->processing_width,
      CV_8UC4, mapped_ptr, buf_params.pitch[0]);
  cvtColor (in_mat, out_mat, CV_RGBA2BGR);

  // To use the converted buffer in CUDA, create an EGLImage and then use
  // CUDA-EGL interop APIs
  if (USE_EGLIMAGE) {
    static EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLImageKHR egl_image;

    // Init egl_display if not initialized
    if (egl_display == EGL_NO_DISPLAY) {
      egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
      if (egl_display == EGL_NO_DISPLAY) {
        flow_ret = GST_FLOW_ERROR;
        goto done;
      }
    }

    // Create an EGLImage from the FD
    egl_image = NvEGLImageFromFd (egl_display, dsexample->conv_dmabuf_fd);
    if (!egl_image) {
      flow_ret = GST_FLOW_ERROR;
      goto done;
    }

    // Use interop APIs cuGraphicsEGLRegisterImage and
    // cuGraphicsResourceGetMappedEglFrame to access the buffer in CUDA

    // Destroy the EGLImage
    NvDestroyEGLImage(egl_display,egl_image);
  }

done:
  if (mapped_ptr)
    NvBufferMemUnMap (dsexample->conv_dmabuf_fd, 0, &mapped_ptr);
  return flow_ret;
}

/**
 * Called when element recieves an input buffer from upstream element.
 */

static bool is_good_crop (const GstDsExample* _this, const ROIMeta_Params& roi_meta)
{
  const int CLASSID_CAR = _this->unique_id == DEFAULT_UNIQUE_ID ? 2 : 0; //TODO Hack! Change unique-id in [ds-example] section of the config file to 14 if you're using 2.0 models

  if (roi_meta.class_id != CLASSID_CAR)
    return false;

  const NvOSD_RectParams& rect = roi_meta.rect_params;

  if (rect.height < 200 || rect.width < 300)
    return false; // Too small an image to contain a legible LP


  return true;
}

static bool are_consistent(const cached_entry_t& cached, const ROIMeta_Params& roi_meta)
{
  if (roi_meta.rect_params.height > cached.rect_params.height*2)
    return false;

  if (roi_meta.rect_params.height < cached.rect_params.height/2)
    return false;

  return true;
}

static GstFlowReturn
gst_dsexample_transform_ip (GstBaseTransform * btrans, GstBuffer * inbuf)
{
  GstDsExample *dsexample = GST_DSEXAMPLE (btrans);
  GstMapInfo in_map_info;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  gdouble scale_ratio;
  DsOpenalprOutput *output;

  int in_dmabuf_fd = 0;

  Mat in_mat;

  dsexample->frame_num++;

  memset (&in_map_info, 0, sizeof (in_map_info));

  if (!gst_buffer_map (inbuf, &in_map_info, GST_MAP_READ)) {
    flow_ret = GST_FLOW_ERROR;
    goto done;
  }
  // Get FD of the buffer
  if (ExtractFdFromNvBuffer (in_map_info.data, &in_dmabuf_fd)) {
    flow_ret = GST_FLOW_ERROR;
    goto done;
  }

  if (dsexample->process_full_frame) {
    NvOSD_RectParams rect_params;
    // Scale the entire frame to processing resolution
    rect_params.left = 0;
    rect_params.top = 0;
    rect_params.width = dsexample->video_info.width;
    rect_params.height = dsexample->video_info.height;

    if (get_converted_mat (dsexample, in_dmabuf_fd, &rect_params,
            *dsexample->cvmat, scale_ratio) != GST_FLOW_OK) {
      flow_ret = GST_FLOW_ERROR;
      goto done;
    }
    
    // Queue the converted buffer for processing
    DsOpenalpr_QueueInput (dsexample->dsexamplelib_ctx, dsexample->cvmat->data);
    // Dequeue the output
    output = DsOpenalpr_DequeueOutput (dsexample->dsexamplelib_ctx);
    // Attach the metadata for the full frame
    attach_metadata_full_frame (dsexample, inbuf, scale_ratio, output);
    g_free (output);

  } 
  else {
    // Using object crops as input to the algorithm. The objects are detected by
    // the primary detector
    GstMeta *gst_meta;
    IvaMeta *ivameta;
    // NOTE: Initializing state to NULL is essential
    gpointer state = NULL;
    BBOX_Params *bbparams;

    // Standard way of iterating through buffer metadata
    while ((gst_meta = gst_buffer_iterate_meta (inbuf, &state)) != NULL) {
      // Check if this metadata is of IvaMeta type
      if (!gst_meta_api_type_has_tag (gst_meta->info->api, _ivameta_quark))
        continue;

      ivameta = (IvaMeta *) gst_meta;
      // Check if the metadata of IvaMeta contains object bounding boxes
      if (ivameta->meta_type != NV_BBOX_INFO)
        continue;

      bbparams = (BBOX_Params *) ivameta->meta_data;
      // Check if these parameters have been set by the primary detector /
      // tracker
      if (bbparams->gie_type != 1) {
        continue;
      }

      // Retire stale entries
      auto& cached_results = *dsexample->cached_results;
      while (!cached_results.empty()
           && cached_results.front().frame_num + CACHE_TTL < dsexample->frame_num ) {
        cached_results.pop_front();
      }


      // Iterate through all the objects
      std::vector<guint> batch;
      for (guint r = 0; r < bbparams->num_rects; r++) {
        ROIMeta_Params *roi_meta = &bbparams->roi_meta[r];

        auto cached = std::find_if (
          cached_results.begin(), cached_results.end(),
          [&] (const cached_entry_t& x)-> bool
          { return roi_meta->tracking_id == x.tracking_id; });

        if (cached != cached_results.end()) {
         //TODO: Move to the head of the queue

          if (are_consistent(*cached, *roi_meta))
          {
            cached->rect_params = roi_meta->rect_params;

            attach_metadata_object (dsexample, roi_meta, &cached->result);

            continue;
          }

          cached_results.erase(cached);
        }

        if (!is_good_crop(dsexample, *roi_meta))
        {
          attach_metadata_object (dsexample, roi_meta);
          continue;
        }

        // Crop and scale the object
        if (get_converted_mat (dsexample, in_dmabuf_fd, &roi_meta->rect_params,
                *dsexample->cvmat, scale_ratio) != GST_FLOW_OK) {
          continue;
        }
        // Queue the object crop for processing
        DsOpenalpr_QueueInput (dsexample->dsexamplelib_ctx,
            dsexample->cvmat->data);

        batch.push_back(r);
      }

      if (!batch.empty()) {
        // Dequeue the output
        DsOpenalprOutput *output = DsOpenalpr_DequeueOutput (dsexample->dsexamplelib_ctx);

        assert(batch.size() == output->frame_results.size());
        // Attach labels for the objects
        for (guint frame = 0; frame < batch.size(); ++frame) {
          guint ix_rect = batch[frame];

          ROIMeta_Params *roi_meta = &bbparams->roi_meta[ix_rect];
          const auto& results = output->frame_results[frame];

          if (attach_metadata_object (dsexample, roi_meta, &results)) {
            cached_results.push_back({*roi_meta, results, dsexample->frame_num});
          }
        }

        g_free (output);
      }
    }
  }

done:
  if (in_dmabuf_fd)
    NvReleaseFd (in_dmabuf_fd);
  gst_buffer_unmap (inbuf, &in_map_info);
  return flow_ret;
}

/**
 * Free the metadata allocated in attach_metadata_full_frame
 */
static void
free_iva_meta (gpointer meta_data)
{
  BBOX_Params *params = (BBOX_Params *) meta_data;
  for (guint i = 0; i < params->num_rects; i++) {
    g_free (params->roi_meta[i].text_params.display_text);
  }
  g_free (params->roi_meta);
  g_free (params);
}

/**
 * Attach metadata for the full frame. We will be adding a new metadata.
 */
static void
attach_metadata_full_frame (GstDsExample * dsexample, GstBuffer * inbuf,
    gdouble scale_ratio, DsOpenalprOutput * output)
{
  if (output->frame_results.size() == 0)
    return;
    
  // Calculate how many plates we have in this batch (each frame may have multiple images)
  int total_meta = 0;
  for (int i = 0; i < output->frame_results.size(); i++)
    total_meta += output->frame_results[i].plates.size();
  
  IvaMeta *ivameta;
  BBOX_Params *bbparams = (BBOX_Params *) g_malloc0 (sizeof (BBOX_Params));
  // Allocate an array of size equal to the number of objects detected
  bbparams->roi_meta =
      (ROIMeta_Params *) g_malloc0 (sizeof (ROIMeta_Params) *
      total_meta);
  // Should be set to 3 for custom elements
  bbparams->gie_type = 3;
  // Use HW for overlaying boxes
  bbparams->nvosd_mode = MODE_HW;
  // Font to be used for label text
  static gchar font_name[] = "Arial";

  // This example app always assumes batch size of 1, so we iterate over the first (and only) frame
  for (gint r = 0; r < output->frame_results.size(); r++)
  for (gint i = 0; i < output->frame_results[r].plates.size(); i++) {
    const alpr::AlprPlateResult& alpr_plate = output->frame_results[r].plates[i];
    
    NvOSD_RectParams & rect_params = bbparams->roi_meta[bbparams->num_rects].rect_params;
    NvOSD_TextParams & text_params = bbparams->roi_meta[bbparams->num_strings].text_params;

    // Compute a rectangular box that fully covers the plate coordinates
    // plate points are topleft, top right, bottom right, bottom left
    // Expand the box by 3%
    float MULTIPLIER = 0.03;
    int left = std::min(alpr_plate.plate_points[0].x, alpr_plate.plate_points[3].x);
    int top = std::min(alpr_plate.plate_points[0].y, alpr_plate.plate_points[1].y);
    int right = std::max(alpr_plate.plate_points[1].x, alpr_plate.plate_points[2].x);
    int bottom = std::max(alpr_plate.plate_points[2].y, alpr_plate.plate_points[3].y);
    left -= (int) (((float)left) * MULTIPLIER);
    top -= (int) (((float)top) * MULTIPLIER);
    right += (int) (((float)right) * MULTIPLIER);
    bottom += (int) (((float)bottom) * MULTIPLIER);
    
    int width = right - left;
    int height = bottom - top;
    
    
    // Assign bounding box coordinates
    rect_params.left = left;
    rect_params.top = top;
    rect_params.width = width;
    rect_params.height = height;

    // Semi-transparent yellow background
    rect_params.has_bg_color = 0;
    rect_params.bg_color = (NvOSD_ColorParams) {
    1, 1, 0, 0.4};
    // Red border of width 6
    rect_params.border_width = 6;
    rect_params.border_color = (NvOSD_ColorParams) {
    1, 0, 0, 1};

    // Scale the bounding boxes proportionally based on how the object/frame was
    // scaled during input
    rect_params.left /= scale_ratio;
    rect_params.top /= scale_ratio;
    rect_params.width /= scale_ratio;
    rect_params.height /= scale_ratio;

    bbparams->num_rects++;

    //std::cout << "PLATE: " << alpr_plate.bestPlate.characters << std::endl;

    // display_text required heap allocated memory
    text_params.display_text = g_strdup (alpr_plate.bestPlate.characters.c_str());
    // Display text above the left top corner of the object
    text_params.x_offset = rect_params.left;
    text_params.y_offset = rect_params.top - 10;
    // Set black background for the text
    text_params.set_bg_clr = 1;
    text_params.text_bg_clr = (NvOSD_ColorParams) {
    0, 0, 0, 1};
    // Font face, size and color
    text_params.font_params.font_name = font_name;
    text_params.font_params.font_size = 11;
    text_params.font_params.font_color = (NvOSD_ColorParams) {
    1, 1, 1, 1};
    bbparams->num_strings++;
  }

  // Attach the BBOX_Params structure as IvaMeta to the buffer. Pass the
  // function to be called when freeing the meta_data
  ivameta = gst_buffer_add_iva_meta_full (inbuf, bbparams, free_iva_meta);
  ivameta->meta_type = NV_BBOX_INFO;
}

/**
 * Only update string label in an existing object metadata. No bounding boxes.
 * We assume only one label per object is generated
 */
static bool
attach_metadata_object (GstDsExample * dsexample, ROIMeta_Params * roi_meta,
    DsOpenalprOutput * output)
{
  return attach_metadata_object (dsexample, roi_meta,
    (output && !output->frame_results.empty()) ? &output->frame_results[0] : nullptr);
}

static bool
attach_metadata_object (GstDsExample * dsexample, ROIMeta_Params * roi_meta,
    const alpr::AlprResults* fr)
{
  std::string LP; assert(LP.empty());
  bool cache_result = true; // Empty results are cached

  if (fr && !fr->plates.empty())
  {

 // The assumption is that there is a single plate on this cropped object (e.g., a single vehicle)
  // however, this is not always true, because some vehicles may have multiple plates (e.g., European trucks)
  // and sometimes there are false positives, so you could have the true plate read along with a bad one
  // Simply taking the most confident result may be a sufficient, simple approach.
  
  // Update the appropriate element of the label_info array. Application knows
  // that output of this element is available at index "unique_id".

    const auto& bestPlate = fr->plates[0].bestPlate;

    LP = bestPlate.characters;
    cache_result = bestPlate.overall_confidence > 50;
  }

  // has_new_info should be set to TRUE whenever adding new/updating
  // information to LabelInfo
  roi_meta->has_new_info = TRUE;

  strcpy (roi_meta->label_info[dsexample->unique_id].str_label, LP.c_str());
  // is_str_label should be set to TRUE indicating that above str_label field is
  // valid
  roi_meta->label_info[dsexample->unique_id].is_str_label = 1;

  return cache_result;
}

/**
 * Boiler plate for registering a plugin and an element.
 */
static gboolean
dsexample_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_dsexample_debug, "dsexample", 0,
      "dsexample plugin");

  return gst_element_register (plugin, "dsexample", GST_RANK_PRIMARY,
      GST_TYPE_DSEXAMPLE);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    dsexample,
    DESCRIPTION, dsexample_plugin_init, VERSION, LICENSE, BINARY_PACKAGE, URL)
