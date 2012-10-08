#ifndef QN_RESOURCE_LAYOUT_H
#define QN_RESOURCE_LAYOUT_H

#define CL_MAX_CHANNELS 4 // TODO: get rid of this definition
#include <QVector>
#include <QStringList>
#include "core/datapacket/media_data_packet.h"

class QN_EXPORT QnResourceLayout
{
public:
    //returns number of audio or video channels device has
    virtual int numberOfChannels() const = 0; // TODO: rename to channelCount()
};

class QnResourceAudioLayout: public QnResourceLayout
{
public:
    struct AudioTrack
    {
        AudioTrack() {} 
        AudioTrack(QnMediaContextPtr ctx, const QString& descr): codecContext(ctx), description(descr) {}

        QnMediaContextPtr codecContext;
        QString description;
    };

    virtual AudioTrack getAudioTrackInfo(int index) const = 0;
};

class QnEmptyResourceAudioLayout: public QnResourceAudioLayout
{
public:
    QnEmptyResourceAudioLayout(): QnResourceAudioLayout() {}
    virtual int numberOfChannels() const override { return 0; }
    virtual AudioTrack getAudioTrackInfo(int index) const override { Q_UNUSED(index); return AudioTrack(); }
};

class QnResourceCustomAudioLayout: public QnResourceAudioLayout
{
public:
    

    QnResourceCustomAudioLayout(): QnResourceAudioLayout() {}

    void addAudioTrack(const AudioTrack& audioTrack) {m_audioTracks << audioTrack; }
    void setNumberOfChannels(int value) { m_audioTracks.resize(value); }
    void setAudioTrackInfo(const AudioTrack& audioTrack, int index) {m_audioTracks[index] = audioTrack; }

    virtual int numberOfChannels() const override { return m_audioTracks.size(); }
    virtual AudioTrack getAudioTrackInfo(int index) const override { return m_audioTracks[index]; }
private:
    QVector<AudioTrack> m_audioTracks;
    
};


class QnResourceVideoLayout: public QnResourceLayout
{
public:
    QnResourceVideoLayout() {}
    virtual ~QnResourceVideoLayout() {}

    // returns maximum width ( in terms of channels 4x1 2x2 1x1 ans so on  )
    virtual int width() const = 0;

    // returns maximum height ( in terms of channels 4x1 2x2 1x1 ans so on  )
    virtual int height() const = 0;

    virtual int h_position(int channel) const = 0; //returns horizontal position of the channel
    virtual int v_position(int channel) const = 0; //returns vertical position of the channel

};

// this is default DeviceVideoLayout for any camera with only one sensor 
class QnDefaultResourceVideoLayout : public QnResourceVideoLayout
{
public:
    QnDefaultResourceVideoLayout() {}
    virtual ~QnDefaultResourceVideoLayout() {}

    virtual int numberOfChannels() const override
    {
        return 1;
    }

    // TODO: use QSize here

    virtual int width() const override
    {
        return 1;
    }

    virtual int height() const override
    {
        return 1;
    }

    // TODO: use QPoint here.

    virtual int h_position(int /*channel*/) const override
    {
        return 0;
    }

    virtual int v_position(int /*channel*/) const override
    {
        return 0;
    }

};

class QnCustomResourceVideoLayout : public QnResourceVideoLayout {
public:
    static QnCustomResourceVideoLayout *fromString(const QString& value)
    {
        QStringList params = value.split(QLatin1Char(';'));
        int width = 1;
        int height = 1;
        QStringList sensors;
        for (int i = 0; i < params.size(); ++i)
        {
            QStringList values = params[i].split(QLatin1Char('='));
            if (values.size() < 2)
                continue;

            if (values[0] == QLatin1String("width")) {
                width = values[1].toInt();
            } else if (values[0] == QLatin1String("height")) {
                height = values[1].toInt();
            } else if (values[0] == QLatin1String("sensors")) {
                sensors = values[1].split(QLatin1Char(','));
            }
        }

        QnCustomResourceVideoLayout* result = new QnCustomResourceVideoLayout(width, height);
        for (int i = 0; i < sensors.size(); ++i)
            result->setChannel(i, sensors[i].toInt());
        return result;
    }

    QnCustomResourceVideoLayout(int width, int height):
        m_width(width),
        m_height(height)
    {
        m_channels.resize(width * height);
    }

    virtual ~QnCustomResourceVideoLayout() {}

    virtual int numberOfChannels() const override
    {
        return m_width * m_height;
    }

    virtual int width() const override
    {
        return m_width;
    }

    virtual int height() const override
    {
        return m_height;
    }

    virtual void setWidth(int value) 
    {
        m_width = value;
        m_channels.resize(m_width * m_height);
    }

    virtual void setHeight(int value)
    {
        m_height = value;
        m_channels.resize(m_width * m_height);
    }

    void setChannel(int h_pos, int v_pos, int channel)
    {
        int index = v_pos * m_width + h_pos;
        if (index >= m_width * m_height)
            return;

        m_channels[index] = channel;
    }

    void setChannel(int index, int channel)
    {
        if (index >= m_width * m_height)
            return;
        m_channels[index] = channel;
    }

    virtual int h_position(int channel) const override
    {
        for (int i = 0; i < m_width * m_height; ++i)
        {
            if (m_channels[i] == channel)
                return i % m_width;
        }

        return 0;
    }

    virtual int v_position(int channel) const override
    {
        for (int i = 0; i < m_width * m_height; ++i)
        {
            if (m_channels[i] == channel)
                return i / m_width;
        }

        return 0;
    }

protected:
    QVector<int> m_channels;
    int m_width;
    int m_height;
};

#endif //QN_RESOURCE_LAYOUT_H
