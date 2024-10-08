// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtMultimedia/QAudioFormat>

#include <nx/media/audio_data_packet.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/vms/client/core/media/voice_spectrum_analyzer.h>

class QnAbstractDataConsumer;

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API DesktopDataProviderBase: public QnAbstractMediaStreamDataProvider
{
public:
    DesktopDataProviderBase(const QnResourcePtr& ptr);

    static void stereoAudioMux(qint16* a1, qint16* a2, int lenInShort);
    static void monoToStereo(qint16* dst, qint16* src, int lenInShort);
    static void monoToStereo(qint16* dst, qint16* src1, qint16* src2, int lenInShort);

    virtual ~DesktopDataProviderBase();

    virtual bool isInitialized() const = 0;
    virtual AudioLayoutConstPtr getAudioLayout() = 0;
    virtual void beforeDestroyDataProvider(QnAbstractMediaDataReceptor* dataProviderWrapper) = 0;
    virtual QString lastErrorStr() const;
    virtual bool readyToStop() const = 0;

protected:
    AVSampleFormat fromQtAudioFormat(const QAudioFormat& format) const;

    QPointer<nx::vms::client::core::VoiceSpectrumAnalyzer> m_soundAnalyzer;
    AudioLayoutPtr m_audioLayout;
    QString m_lastErrorStr;
};

} // namespace nx::vms::client::core
