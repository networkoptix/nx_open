/**********************************************************
* 2 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef CAMERA_PLUGIN_TYPES_H
#define CAMERA_PLUGIN_TYPES_H


namespace nxcip
{
    enum CompressionType
    {
        CODEC_ID_NONE,
        CODEC_ID_MPEG2VIDEO,
        CODEC_ID_H263,
        CODEC_ID_MJPEG,
        CODEC_ID_MPEG4,
        CODEC_ID_H264,
        CODEC_ID_THEORA,
        CODEC_ID_PNG,
        CODEC_ID_GIF,

        CODEC_ID_MP2 = 0x15000,
        CODEC_ID_MP3,
        CODEC_ID_AAC,
        CODEC_ID_AC3,
        CODEC_ID_DTS,
        CODEC_ID_VORBIS,
    };
}

#endif  //CAMERA_PLUGIN_TYPES_H
