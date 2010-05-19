#include "uvideo_wnd.h"


#include <QPainter>

VideoWindow::VideoWindow(const CLDeviceVideoLayout* layout):
CLVideoWindow(layout),
m_selected(false)
{

}

void VideoWindow::setPosition(int pos)
{
	m_position = pos;
}

int VideoWindow::getPosition() const
{
	return m_position;
}

void VideoWindow::setSelected(bool sel)
{
	m_selected = sel;
}


void VideoWindow::drawStuff(QPainter* painter)
{
	CLVideoWindow::drawStuff(painter);

	if (m_selected)
		drawSelection(painter);


}

void VideoWindow::drawSelection(QPainter* painter)
{

}

