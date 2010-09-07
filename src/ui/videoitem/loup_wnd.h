#ifndef loupe_wnd_h_1841
#define loupe_wnd_h_1841

#include "videodisplay\video_window.h"


class LoupeWnd : public CLVideoWindow
{
public:
	LoupeWnd(int width, int height); // width and height in viewport coordinate system
	~LoupeWnd();
protected:
	void drawStuff(QPainter* painter);


};

#endif // loupe_wnd_h_1841