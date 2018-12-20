#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QOpenGLContext>

#include <nx/streaming/video_data_packet.h>

#include "abstract_render_context_synchronizer.h"

namespace nx {
namespace media {
class AbstractAudioDecoder;
typedef std::unique_ptr<AbstractAudioDecoder> AudioDecoderPtr;

/*
* This class allows to register various implementations of audio decoders. Exact list of decoders can be registered in runtime.
*/
class AudioDecoderRegistry
{
public:
    static AudioDecoderRegistry* instance();
    /*
    * \returns optimal audio decoder (in case of any) compatible with such frame. Returns null if no compatible decoder found.
    */
    AudioDecoderPtr createCompatibleDecoder(const AVCodecID codec);

    /*
    * \returns true if compatible video decoder found
    */
    bool hasCompatibleDecoder(const AVCodecID codec);

    /** Register video decoder plugin */
    template <class Decoder>
    void addPlugin()
    {
        m_plugins.push_back(MetadataImpl<Decoder>());
    }

private:
    struct Metadata
    {
        Metadata() {}

        std::function<AbstractAudioDecoder* ()> instance;
        std::function<bool(const AVCodecID codec)> isCompatible;
    };

    template <class Decoder>
    struct MetadataImpl : public Metadata
    {
        MetadataImpl()
        {
            instance = []() { return new Decoder(); };
            isCompatible = &Decoder::isCompatible;
        }
    };

    std::vector<Metadata> m_plugins;
};

}
}
