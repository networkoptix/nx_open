#ifndef device_video_layout_h_2143
#define device_video_layout_h_2143

#define CL_MAX_CHANNELS 4
#include <QVector>

class QN_EXPORT QnDeviceLayout
{
public:
    //returns number of audio or video channels device has
    virtual unsigned int numberOfChannels() const = 0;
};

class QnResourceAudioLayout: public QnDeviceLayout
{
public:
    struct AudioTrack
    {
        AudioTrack(): codec(CODEC_ID_NONE) {}
        CodecID codec;
        QString description;
    };

    virtual AudioTrack getAudioTrackInfo(int index) const = 0;
};

class QnEmptyAudioLayout: public QnResourceAudioLayout
{
public:
    QnEmptyAudioLayout(): QnResourceAudioLayout() {}
    virtual unsigned int numberOfChannels() const { return 0; }
    virtual AudioTrack getAudioTrackInfo(int /*index*/) const { return AudioTrack(); }
};

class QnVideoResourceLayout: public QnDeviceLayout
{
public:
    QnVideoResourceLayout() {}
    virtual ~QnVideoResourceLayout() {}

    // returns maximum width ( in terms of channels 4x1 2x2 1x1 ans so on  )
    virtual unsigned int width() const = 0;

    // returns maximum height ( in terms of channels 4x1 2x2 1x1 ans so on  )
    virtual unsigned int height() const = 0;

    virtual unsigned int h_position(unsigned int channel) const = 0; //returns horizontal position of the channel
    virtual unsigned int v_position(unsigned int channel) const = 0; //returns vertical position of the channel

};

// this is default DeviceVideoLayout for any camera with only one sensor 
class QnDefaultDeviceVideoLayout : public QnVideoResourceLayout
{
public:
    QnDefaultDeviceVideoLayout() {}
    virtual ~QnDefaultDeviceVideoLayout() {}
    //returns number of video channels device has
    virtual unsigned int numberOfChannels() const
    {
        return 1;
    }

    virtual unsigned int width() const
    {
        return 1;
    }

    virtual unsigned int height() const
    {
        return 1;
    }

    virtual unsigned int h_position(unsigned int /*channel*/) const
    {
        return 0;
    }

    virtual unsigned int v_position(unsigned int /*channel*/) const
    {
        return 0;
    }

};

class CLCustomDeviceVideoLayout : public QnVideoResourceLayout
{
public:
    static CLCustomDeviceVideoLayout* fromString(const QString& value)
    {
        QStringList params = value.split(';');
        int width = 1;
        int height = 1;
        QStringList sensors;
        for (int i = 0; i < params.size(); ++i)
        {
            QStringList values = params[i].split('=');
            if (values.size() < 2)
                continue;
            if (values[0] == "width")
                width = values[1].toInt();
            else if (values[0] == "height")
                height = values[1].toInt();
            else if (values[0] == "sensors") 
                sensors = values[1].split(',');
        }
        CLCustomDeviceVideoLayout* result = new CLCustomDeviceVideoLayout(width, height);
        for (int i = 0; i < sensors.size(); ++i)
            result->setChannel(i, sensors[i].toInt());
        return result;
    }

    CLCustomDeviceVideoLayout(int width, int height):
        m_width(width),
        m_height(height)
    {
        m_channels.resize(width*height);
    }
    virtual ~CLCustomDeviceVideoLayout(){}
    //returns number of video channels device has
    virtual unsigned int numberOfChannels() const
    {
        return m_width*m_height;
    }

    virtual unsigned int width() const
    {
        return m_width;
    }

    virtual unsigned int height() const
    {
        return m_height;
    }

    virtual void setWidth(int value) 
    {
        m_width = value;
        m_channels.resize(m_width*m_height);
    }

    virtual void setHeight(int value)
    {
        m_height = value;
        m_channels.resize(m_width*m_height);
    }

    void setChannel(int h_pos, int v_pos, int channel)
    {
        int index = v_pos*m_width + h_pos;
        if (index>=m_width*m_height)
            return;

        m_channels[index] = channel;
    }

    void setChannel(int index, int channel)
    {
        if (index>=m_width*m_height)
            return;
        m_channels[index] = channel;
    }

    virtual unsigned int h_position(unsigned int channel) const
    {
        for (int i = 0; i < m_width*m_height; ++i)
        {
            if (m_channels[i]==channel)
                return i%m_width;
        }

        return 0;
    }

    virtual unsigned int v_position(unsigned int channel) const
    {
        for (int i = 0; i < m_width*m_height; ++i)
        {
            if (m_channels[i]==channel)
                return i/m_width;
        }

        return 0;
    }

protected:

    QVector<int> m_channels;
    int m_width;
    int m_height;
};

#endif //device_video_layout_h_2143
