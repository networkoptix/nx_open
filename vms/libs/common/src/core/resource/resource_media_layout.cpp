#include "resource_media_layout.h"

QnCustomResourceVideoLayout::QnCustomResourceVideoLayout(const QSize &size) :
    m_size(size),
    m_cachedChannelCount(boost::none)
{
    m_channels.resize(m_size.width() * m_size.height());
}

QnCustomResourceVideoLayoutPtr QnCustomResourceVideoLayout::fromString(const QString &value)
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

QString QnCustomResourceVideoLayout::toString() const 
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

int QnCustomResourceVideoLayout::channelCount() const 
{
    if (m_cachedChannelCount)
        return m_cachedChannelCount.get();
    
    int count = 0;
    for (const auto& channel: m_channels)
    {
        if (channel >= 0)
            ++count;
    }

    m_cachedChannelCount = count;
    return count;
}

QSize QnCustomResourceVideoLayout::size() const 
{
    return m_size;
}

void QnCustomResourceVideoLayout::setSize(const QSize &size)
{
    m_size = size;
    m_channels.resize(m_size.width() * m_size.height());
}

void QnCustomResourceVideoLayout::setChannel(int x, int y, int channel)
{
    setChannel(y * m_size.width() + x, channel);
}

void QnCustomResourceVideoLayout::setChannel(int index, int channel)
{
    m_cachedChannelCount = boost::none;
    if (index >= m_size.width() * m_size.height())
        return;
    m_channels[index] = channel;
}

QPoint QnCustomResourceVideoLayout::position(int channel) const 
{
    for (int i = 0; i < m_size.width() * m_size.height(); ++i) 
        if (m_channels[i] == channel)
            return QPoint(i % m_size.width(), i / m_size.width());

    return QPoint();
}

QVector<int> QnCustomResourceVideoLayout::getChannels() const
{
    return m_channels;
}

void QnCustomResourceVideoLayout::setChannels(const QVector<int>& value)
{
    m_channels = value;
}
