/*
  vmux_iface.h

  Defines an interface for getting video frames
*/


#ifndef _VMUX_H
#define _VMUX_H

#define VMUX_NUM_ENCODERS 4

typedef struct {
    unsigned int packet_type;
#define VPKT_INFO  1 // payload is vmux_stream_info_t
#define VPKT_FRAME 2 // payload is vmux_frame_t
    unsigned int payload_length; /* either 
				    sizeof vmux_stream_info_t or
				    sizeof vmux_frame_t */
} vmux_pkt_hdr_t;

typedef struct {
    unsigned int        stream_id;
    unsigned int	session_id;	//use 32-bit session id
    unsigned int	frame_num;      // monotonically increasing number from encoder
    unsigned int	pic_type;
/*encode pic_type */
#define VMUX_IDR_FRAME		(1)
#define VMUX_I_FRAME		(2)
#define VMUX_P_FRAME		(3)
#define VMUX_B_FRAME		(4)
#define VMUX_Y_FRAME            (5)
    unsigned int	PTS;         // presentation time stamp
    unsigned int        stream_end;  // 0: normal stream frames,  1: stream end null frame
} vmux_info_t;

typedef struct {
    vmux_info_t     vmux_info;
    unsigned char  *frame_addr; /* the actual address in stream buffer */
    unsigned int    frame_size;	 /* the actual output frame size , including CAVLC */
} vmux_frame_t;	

/* stream description info */
typedef struct {
    int width;
    int height;
    int enc_type;
#define VMUX_ENC_TYPE_NONE (0) // encoder disabled 
#define VMUX_ENC_TYPE_H264 (1) // h.264
#define VMUX_ENC_TYPE_MJPG (2) // jpeg
#define VMUX_ENC_TYPE_Y    (3) // raw Y data
#define VMUX_ENC_TYPE_YUV  (4) // YUV
    int pitch;                 // >= width
} vmux_stream_info_t;

typedef struct {
    unsigned char chroma_format;
#define VMUX_CHROMA_YUV422 0
#define VMUX_CHROMA_YUV420 1
#define VMUX_CHROMA_MONO   2
    unsigned char quality; // 1 ~ 100, 100 is highest quality
} vmux_jpeg_info_t;

#ifdef EXTRA_THREAD
typedef struct _VidFifo {
    struct _VidFifo *flink;
    vmux_frame_t info;
} VidFifo;
#endif

class Vmux {

 public:
    Vmux();
    ~Vmux();

    /* Start video on stream 0-3.  Call this FIRST. */
    int StartVideo (int stream);
    /* 
       Stream 0-3 are the hardware encoders and deliver either
       h.264 or jpeg.
       Stream 4 is the Y plane of the largest source resolution downsampled
       by 4 in each dimension, e.g. 1920/4 x 1080/4 = 480x270.
       Stream 5 is the Y plane of the smaller source resolution downsampled
       by 4 in each dimension, e.g. 640/4 x 360/4 = 160x90
       Stream 4 = larger Y frame
       Stream 5 = smaller Y frame
       Stream 6 = YUV of smaller source resolution 
    */
#define Y_STREAM_BIG   4
#define Y_STREAM_SMALL 5
#define YUV_STREAM     6
    /* Read the next video frame.  This will block until a frame
       is available.  This frame points into our free-running encoder
       fifo so grab it quick! */
    int GetFrame (vmux_frame_t *frame_info);
    /* Returns a file descriptor you can use to check for readability
       (fionread, select, and friends) if you're concerned about the
       blocking frame read. */
    int GetFD (void);
    /* Retrieve meta data about stream.
       stream = 0~3
       stream_info gets stream info data
       *info_size 
         IN: size of stream_info buffer
	 OUT: # of bytes written
       Returns 0 on success, !0 on failure. */
    int GetStreamInfo (int stream, vmux_stream_info_t *stream_info,
		       int *info_size);
    /* Set downsample to 1, 2, or 4.
       Not valid with h264 or jpeg streams.
       Returns 0 on success or -1 if ds is out of range. */
    int SetDownsample (int ds);

    /* Retrieve meta data about the jpeg stream
       stream = 0~3
       jpeg_info gets stream info data
       *info_size 
         IN: size of jpeg_info buffer
	 OUT: # of bytes written
       Returns 0 on success, !0 on failure. */
    int GetJpegInfo   (int stream, vmux_jpeg_info_t *jpeg_info,
		       int *info_size);
#ifdef EXTRA_THREAD
    void VideoThread (void);
#endif

 private:
    char *imgbf;  // buffer to hold image
    int imgbf_size; // size of imgbf
    vmux_stream_info_t stream_info;
    char got_stream_info;
    int infile;
    //    unsigned char *bsaddr;
    char *fifo_name;
    int m_stream;
    unsigned int fnum;
    int downsample;

#ifdef EXTRA_THREAD
    HANDLE video_mutex;
    HANDLE video_event;
    HANDLE video_thread;
    bool Quit;
    VidFifo *frames; // video frame fifo
    VidFifo *last_frame; // address of last entry
    VidFifo *last_used; // entry last handed out
    DWORD frames_avail;
    int AddFrame (vmux_frame_t *frame_info, char *data);
    int GetFrameInternal (vmux_frame_t *frame_info);
#endif // EXTRA_THREAD
};
#endif // _VMUX_H
