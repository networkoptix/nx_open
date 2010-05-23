#ifndef uvideo_wnd_h_1951
#define uvideo_wnd_h_1951

#include "videodisplay/video_window.h"

class VideoWindow : public CLVideoWindow
{
public:
	VideoWindow(const CLDeviceVideoLayout* layout, int max_width, int max_height);



	void setSelected(bool sel);


protected:
	virtual void drawStuff(QPainter* painter);
private:

	void drawSelection(QPainter* painter);


private:
	bool m_selected;
	
};


#endif //uvideo_wnd_h_1951