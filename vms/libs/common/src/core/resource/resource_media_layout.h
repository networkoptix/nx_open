#ifndef QN_RESOURCE_LAYOUT_H
#define QN_RESOURCE_LAYOUT_H

#include <memory>

#include <boost/optional/optional.hpp>

#include <QtCore/QVector>
#include <QtCore/QStringList>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QSharedPointer>

#include <nx/streaming/media_context.h>

#include <nx/utils/literal.h>

#include <common/common_globals.h>

class QnResourceLayout {
public:
    QnResourceLayout() {}
    virtual ~QnResourceLayout() {}

    /**
     * @return Number of audio or video channels a device has.
     */
    virtual int channelCount() const = 0;

private:
    Q_DISABLE_COPY(QnResourceLayout);
};


class QnResourceAudioLayout: public QnResourceLayout {
public:
    struct AudioTrack {
        AudioTrack() {}
        AudioTrack(const QnConstMediaContextPtr& codecContext, const QString &description): codecContext(codecContext), description(description) {}

        QnConstMediaContextPtr codecContext;
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
     * @return Size of the channel matrix.
     */
    virtual QSize size() const = 0;

    /**
     * @return Position of the given channel in a channel matrix.
     */
    virtual QPoint position(int channel) const = 0;

    /**
     * @return Matrix data assumed to be in row-major order.
     */
    virtual QVector<int> getChannels() const = 0;

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

    virtual QVector<int> getChannels() const override
    {
        return QVector<int>() << 0;
    }
};


class QnCustomResourceVideoLayout;

typedef QSharedPointer<QnCustomResourceVideoLayout> QnCustomResourceVideoLayoutPtr;
typedef QSharedPointer<const QnCustomResourceVideoLayout> QnConstCustomResourceVideoLayoutPtr;

class QnCustomResourceVideoLayout : public QnResourceVideoLayout {
public:

    QnCustomResourceVideoLayout(const QSize &size);

    static QnCustomResourceVideoLayoutPtr fromString(const QString &value);

    virtual QString toString() const override;

    virtual int channelCount() const override;

    virtual QSize size() const override;

    void setSize(const QSize &size);

    void setChannel(int x, int y, int channel);

    void setChannel(int index, int channel);

    virtual QPoint position(int channel) const override;

    virtual QVector<int> getChannels() const override;

    void setChannels(const QVector<int>& value);

protected:
    QVector<int> m_channels;
    QSize m_size;
    mutable boost::optional<int> m_cachedChannelCount;
};

#endif // QN_RESOURCE_LAYOUT_H
