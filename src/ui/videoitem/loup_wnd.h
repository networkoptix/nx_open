#ifndef loupe_wnd_h_1841
#define loupe_wnd_h_1841

#include "video_wnd_item.h"

class LoupeWnd : public CLVideoWindowItem
{
public:
	LoupeWnd(int width, int height); // width and height in viewport coordinate system
	~LoupeWnd();
protected:
	void drawStuff(QPainter* painter);

};

#endif // loupe_wnd_h_1841