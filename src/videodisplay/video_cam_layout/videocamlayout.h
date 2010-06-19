#ifndef video_wnd_layout_h1750
#define video_wnd_layout_h1750

class CLVideoCamera;
class CLDeviceVideoLayout;

// class provides just mapping mechanism between slots and cams
class VideoCamerasLayout
{
public:
	VideoCamerasLayout(unsigned int max_rows, unsigned int max_slots);
	~VideoCamerasLayout();

	// returns next available position; returns -1 if not found;
	int getNextAvailablePos(const CLDeviceVideoLayout* layout) const;

	void getXY(unsigned int pos, int& x, int& y) const;

	CLVideoCamera*  getFirstCam() const;

	int getPos(const CLVideoCamera* cam) const;

	// return true if succedded 
	bool getXY(const CLVideoCamera* cam, int& x, int& y) const;

	void addCamera(int position, CLVideoCamera* cam);
	void removeCamera(CLVideoCamera* cam);

	CLVideoCamera* getNextLeftCam(const CLVideoCamera* curr) const;
	CLVideoCamera* getNextRightCam(const CLVideoCamera* curr) const;
	CLVideoCamera* getNextTopCam(const CLVideoCamera* curr) const;
	CLVideoCamera* getNextBottomCam(const CLVideoCamera* curr) const;


private:
	// return -1 if not found
	int getNextAvalableSlot(unsigned int start_point, unsigned long min_energy)const;

	// return -1 if not found
	int getNextAvailablePosWidthHeight(unsigned int start_point, int width, int height, unsigned long &min_energy) const;
private:

	CLVideoCamera** m_cams;

	unsigned long* m_potential_energy;
	unsigned int m_width;
	unsigned int m_height;
	unsigned int m_slots;

	
};

#endif //video_wnd_layout_h1750