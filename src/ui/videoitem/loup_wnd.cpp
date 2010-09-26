#include "loup_wnd.h"

LoupeWnd::LoupeWnd(int width, int height):
CLVideoWindowItem(0, 0, width, height)
{
	m_videolayout = new CLDefaultDeviceVideoLayout();
}


LoupeWnd::~LoupeWnd()
{
	delete m_videolayout;
}

void LoupeWnd::drawStuff(QPainter* painter)
{
	painter->fillRect(QRect(0, 0, width(), height()),
		QColor(255, 0, 0, 85));
}
