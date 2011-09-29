#ifndef QnMediaResourceLayout_h_2143
#define QnMediaResourceLayout_h_2143

#define CL_MAX_CHANNELS 4

class QnMediaResourceLayout
{
public:
    QnMediaResourceLayout(){};
    virtual ~QnMediaResourceLayout() {}
    //returns number of video channels device has
    virtual unsigned int numberOfVideoChannels() const = 0;
    virtual unsigned int numberOfAudioChannels() const = 0;

	// returns maximum width ( in terms of channels 4x1 2x2 1x1 ans so on  )
	virtual unsigned int width() const = 0;

	// returns maximum height ( in terms of channels 4x1 2x2 1x1 ans so on  )
	virtual unsigned int height() const = 0;

	virtual unsigned int h_position(unsigned int channel) const = 0; //returns horizontal position of the channel
	virtual unsigned int v_position(unsigned int channel) const = 0; //returns vertical position of the channel

};

// this is default DeviceVideoLayout for any camera with only one sensor
class QnDefaultMediaResourceLayout : public QnMediaResourceLayout
{
public:
	QnDefaultMediaResourceLayout(){};
	virtual ~QnDefaultMediaResourceLayout() {}
	//returns number of video channels device has
	virtual unsigned int numberOfVideoChannels() const
	{
		return 1;
	}

    virtual unsigned int numberOfAudioChannels() const
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

class QnCustomDeviceVideoLayout : public QnMediaResourceLayout
{
public:
	QnCustomDeviceVideoLayout(int width, int height):
	m_width(width),
	m_height(height)
	{
		m_channels = new int[m_width*m_height];
	};
	virtual ~QnCustomDeviceVideoLayout()
	{
		delete[] m_channels;
	};
	//returns number of video channels device has
	virtual unsigned int numberOfVideoChannels() const
	{
		return m_width*m_height;
	}

    virtual unsigned int numberOfAudioChannels() const
    {
        return 1;
    }

	virtual unsigned int width() const
	{
		return m_width;
	}

	virtual unsigned int height() const
	{
		return m_height;
	}

	void setChannel(int h_pos, int v_pos, int channel)
	{
		int index = v_pos*m_width + h_pos;
		if (index>=m_width*m_height)
			return;

		m_channels[index] = channel;
	}

	virtual unsigned int h_position(unsigned int channel) const
	{
		for (int i = 0; i < m_width*m_height; ++i)
		{
			if (uint(m_channels[i]) == channel)
				return i%m_width;
		}

		return 0;
	}

	virtual unsigned int v_position(unsigned int channel) const
	{
		for (int i = 0; i < m_width*m_height; ++i)
		{
			if (uint(m_channels[i]) == channel)
				return i/m_width;
		}

		return 0;
	}

protected:

	int* m_channels;
	int m_width;
	int m_height;
};

#endif //device_video_layout_h_2143
