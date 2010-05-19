#ifndef device_video_layout_h_2143
#define device_video_layout_h_2143

#define CL_MAX_CHANNELS 4

class CLDeviceVideoLayout
{
public:
	CLDeviceVideoLayout(){};
	virtual ~CLDeviceVideoLayout()=0{};
	//returns number of video channels device has
	virtual unsigned int numberOfChannels() const = 0;

	// returns maximum width ( in terms of channels 4x1 2x2 1x1 ans so on  )
	virtual unsigned int width() const = 0; 

	// returns maximum height ( in terms of channels 4x1 2x2 1x1 ans so on  )
	virtual unsigned int height() const = 0; 

	virtual unsigned int h_position(unsigned int channel) = 0; //returns horizontal position of the channel
	virtual unsigned int v_position(unsigned int channel) = 0; //returns vertical position of the channel

};

// this is default DeviceVideoLayout for any camera with only one sensor 
class CLDefaultDeviceVideoLayout : public CLDeviceVideoLayout
{
public:
	CLDefaultDeviceVideoLayout(){};
	virtual ~CLDefaultDeviceVideoLayout(){};
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

	virtual unsigned int h_position(unsigned int channel)
	{
		return 0;
	}

	virtual unsigned int v_position(unsigned int channel)
	{
		return 0;
	}

};


#endif //device_video_layout_h_2143