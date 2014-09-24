#ifndef QN_RESOURCE_LAYOUT_H
#define QN_RESOURCE_LAYOUT_H

#include <memory>

#include <QtCore/QVector>
#include <QtCore/QStringList>
#include <QtCore/QPoint>
#include <QtCore/QSize>

#include <core/datapacket/abstract_media_context.h>

#include <common/common_globals.h>

class QnResourceLayout {
public:
    QnResourceLayout() {}
    virtual ~QnResourceLayout() {}

    /** 
     * \returns                         Number of audio or video channels a device has.
     */
    virtual int channelCount() const = 0;

private:
    Q_DISABLE_COPY(QnResourceLayout);
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

typedef QSharedPointer<QnResourceAudioLayout> QnResourceAudioLayoutPtr;
typedef QSharedPointer<const QnResourceAudioLayout> QnConstResourceAudioLayoutPtr;


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

typedef QSharedPointer<QnResourceCustomAudioLayout> QnResourceCustomAudioLayoutPtr;
typedef QSharedPointer<const QnResourceCustomAudioLayout> QnConstResourceCustomAudioLayoutPtr;


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

    virtual QString toString() const { return QString(); }
};

typedef QSharedPointer<QnResourceVideoLayout> QnResourceVideoLayoutPtr;
typedef QSharedPointer<const QnResourceVideoLayout> QnConstResourceVideoLayoutPtr;


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


class QnCustomResourceVideoLayout;

typedef QSharedPointer<QnCustomResourceVideoLayout> QnCustomResourceVideoLayoutPtr;
typedef QSharedPointer<const QnCustomResourceVideoLayout> QnConstCustomResourceVideoLayoutPtr;

class QnCustomResourceVideoLayout : public QnResourceVideoLayout {
public:
    static QnCustomResourceVideoLayoutPtr fromString(const QString &value)
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

        QnCustomResourceVideoLayoutPtr result( new QnCustomResourceVideoLayout(QSize(width, height)) );
        for (int i = 0; i < sensors.size(); ++i)
            result->setChannel(i, sensors[i].toInt());
        return result;
    }

    virtual QString toString() const override
    {
        QString result(lit("width=%1;height=%2;sensors=%3"));
        QString sensors;
        for (int i = 0; i < m_channels.size(); ++i) 
        {
            if (i > 0)
                sensors += L',';
            sensors += QString::number(m_channels[i]);
        }
        return result.arg(m_size.width()).arg(m_size.height()).arg(sensors);
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

    QVector<int> getChannels() const            { return m_channels; }
    void setChannels(const QVector<int>& value) { m_channels = value; }

protected:
    QVector<int> m_channels;
    QSize m_size;
};

#endif // QN_RESOURCE_LAYOUT_H
