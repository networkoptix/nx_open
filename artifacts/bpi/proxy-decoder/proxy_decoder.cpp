#include "proxy_decoder.h"

#include "proxy_decoder_utils.h"

ProxyDecoder* ProxyDecoder::create(int frameW, int frameH)
{
    conf.reload();
    conf.skipNextReload(); //< Each of the methods below calls conf.reload().
    if (conf.disable || conf.enableStub)
        return createStub(frameW, frameH);
    else
        return createImpl(frameW, frameH);
}

void ProxyDecoder::getMaxResolution(int* outFrameW, int* outFrameH)
{
    // According to documentation for the A20 chip.
    *outFrameW = 3840;
    *outFrameH = 2160;
}
