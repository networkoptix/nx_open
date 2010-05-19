#ifndef uvideo_wnd_h_1951
#define uvideo_wnd_h_1951

#include "videodisplay/video_window.h"

class VideoWindow : public CLVideoWindow
{
public:
	VideoWindow(const CLDeviceVideoLayout* layout);

	void setPosition(int pos) ;
	int getPosition() const;

	void setSelected(bool sel);


protected:
	virtual void drawStuff(QPainter* painter);
private:

	void drawSelection(QPainter* painter);


private:

	int m_position;
	bool m_selected;
	
};


#endif //uvideo_wnd_h_1951