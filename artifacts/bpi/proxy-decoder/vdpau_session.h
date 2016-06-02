#pragma once

#include <vector>

#include "vdpau_utils.h"

class VdpauSession
{
public:
    VdpauSession(int frameWidth, int frameHeight);
    virtual ~VdpauSession();

    void displayVideoSurface(
        VdpVideoSurface videoSurface, int x, int y, int width, int height);

    VdpDevice vdpDevice() const { return m_vdpDevice; }

private:
    VdpOutputSurface obtainOutputSurface();
    void releaseOutputSurface(VdpOutputSurface outputSurface) const;
    VdpRect obtainDestinationVideoRect(int x, int y, int width, int height);

private:
    int m_frameWidth;
    int m_frameHeight;

    int m_outputSurfaceIndex = 0;
    std::vector<VdpOutputSurface> m_outputSurfaces;

    VdpDevice m_vdpDevice = VDP_INVALID_HANDLE;
    VdpVideoMixer m_vdpMixer = VDP_INVALID_HANDLE;
    VdpPresentationQueueTarget m_vdpTarget = VDP_INVALID_HANDLE;
    VdpPresentationQueue m_vdpQueue = VDP_INVALID_HANDLE;

    bool m_videoRectReported = false;
};
