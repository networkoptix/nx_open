// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QVector>
#include <QtCore/QStringList>
#include <QtCore/QPoint>
#include <QtCore/QSize>

#include <core/resource/audio_layout.h>

#include "resource_media_layout_fwd.h"

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

class NX_VMS_COMMON_API QnCustomResourceVideoLayout : public QnResourceVideoLayout {
public:

    QnCustomResourceVideoLayout(const QSize &size);

    /**
     * Converts from a string representation produced by toString(). Ignores all errors, setting
     * to 0 all values that are omitted or cannot be parsed.
     */
    static QnCustomResourceVideoLayoutPtr fromString(const QString &value);

    /**
     * Converts to a string representation which is considered to be an implementation detail, and
     * is guaranteed to be parsed by fromString().
     */
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
    mutable std::optional<int> m_cachedChannelCount = std::nullopt;
};
