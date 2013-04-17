#ifndef QN_RESOURCE_LAYOUT_H
#define QN_RESOURCE_LAYOUT_H

#define CL_MAX_CHANNELS 4 // TODO: get rid of this definition

#include <QtCore/QVector>
#include <QtCore/QStringList>
#include <QtCore/QPoint>
#include <QtCore/QSize>

#include <core/datapacket/abstract_data_packet.h>


class QnResourceLayout {
public:
    virtual ~QnResourceLayout() {}

    /** 
     * \returns                         Number of audio or video channels a device has.
     */
    virtual int channelCount() const = 0;
};


class QnResourceAudioLayout: public QnResourceLayout {
public:
    struct AudioTrack {
        AudioTrack() {} 
        AudioTrack(QnAbstractMediaContextPtr codecContext, const QString &description): codecContext(codecContext), description(description) {}

        QnAbstractMediaContextPtr codecContext;
        QString description;
    };

    virtual AudioTrack getAudioTrackInfo(int index) const = 0;
};


class QnEmptyResourceAudioLayout: public QnResourceAudioLayout {
public:
    QnEmptyResourceAudioLayout() {}
    virtual int channelCount() const override { return 0; }
    virtual AudioTrack getAudioTrackInfo(int) const override { return AudioTrack(); }
};


class QnResourceCustomAudioLayout: public QnResourceAudioLayout {
public:
    QnResourceCustomAudioLayout(): QnResourceAudioLayout() {}

    void addAudioTrack(const AudioTrack &audioTrack) { m_audioTracks << audioTrack; }
    void setAudioTrack(int index, const AudioTrack &audioTrack) { m_audioTracks[index] = audioTrack; }

    virtual int channelCount() const override { return m_audioTracks.size(); }
    virtual AudioTrack getAudioTrackInfo(int index) const override { return m_audioTracks[index]; }

private:
    QVector<AudioTrack> m_audioTracks;
};


class QnResourceVideoLayout: public QnResourceLayout {
public:
    QnResourceVideoLayout() {}

    /**
     * \returns                         Size of the channel matrix. 
     */
    virtual QSize size() const = 0;

    /**
     * \returns                         Position of the given channel in a channel matrix.
     */
    virtual QPoint position(int channel) const = 0;
};


/**
 * Default video layout for any camera with only one sensor.
 */
class QnDefaultResourceVideoLayout : public QnResourceVideoLayout {
public:
    QnDefaultResourceVideoLayout() {}

    virtual int channelCount() const override {
        return 1;
    }

    virtual QSize size() const override {
        return QSize(1, 1);
    }

    virtual QPoint position(int) const override {
        return QPoint(0, 0);
    }
};


class QnCustomResourceVideoLayout : public QnResourceVideoLayout {
public:
    static QnCustomResourceVideoLayout *fromString(const QString &value)
    {
        QStringList params = value.split(QLatin1Char(';'));
        int width = 1;
        int height = 1;
        QStringList sensors;
        for (int i = 0; i < params.size(); ++i) {
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

        QnCustomResourceVideoLayout *result = new QnCustomResourceVideoLayout(QSize(width, height));
        for (int i = 0; i < sensors.size(); ++i)
            result->setChannel(i, sensors[i].toInt());
        return result;
    }

    QnCustomResourceVideoLayout(const QSize &size):
        m_size(size)
    {
        m_channels.resize(m_size.width() * m_size.height());
    }

    virtual int channelCount() const override {
        return m_size.width() * m_size.height();
    }

    virtual QSize size() const override {
        return m_size;
    }

    void setSize(const QSize &size) {
        m_size = size;
        m_channels.resize(m_size.width() * m_size.height());
    }

    void setChannel(int x, int y, int channel) {
        setChannel(y * m_size.width() + x, channel);
    }

    void setChannel(int index, int channel) {
        if (index >= m_size.width() * m_size.height())
            return;
        m_channels[index] = channel;
    }

    virtual QPoint position(int channel) const override {
        for (int i = 0; i < m_size.width() * m_size.height(); ++i) 
            if (m_channels[i] == channel)
                return QPoint(i % m_size.width(), i / m_size.width());

        return QPoint();
    }

protected:
    QVector<int> m_channels;
    QSize m_size;
};

#endif // QN_RESOURCE_LAYOUT_H
