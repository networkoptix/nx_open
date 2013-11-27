/**********************************************************
* 27 nov 2013
* akolesnikov
***********************************************************/

#ifdef _WIN32

#include "vmux_iface.h"



Vmux::Vmux()
{
}

Vmux::~Vmux()
{
}

int Vmux::StartVideo (int stream)
{
    return 0;
}

int Vmux::GetFrame (vmux_frame_t *frame_info)
{
    return 0;
}

int Vmux::GetFD (void)
{
    return 0;
}

int Vmux::GetStreamInfo (int stream, vmux_stream_info_t *stream_info,
            int *info_size)
{
    return 0;
}

int Vmux::SetDownsample (int ds)
{
    return 0;
}

int Vmux::GetJpegInfo   (int stream, vmux_jpeg_info_t *jpeg_info,
            int *info_size)
{
    return 0;
}

#endif  //_WIN32
