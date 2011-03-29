#include "intro_video_wnd.h"

CLIntroVideoitem::CLIntroVideoitem(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height,
                                   QString name):
CLVideoWindowItem(view, layout, max_width, max_width*9/16, name)
{
    goToSteadyMode(true, true);
}

CLIntroVideoitem::~CLIntroVideoitem()
{

}


void CLIntroVideoitem::drawStuff(QPainter* painter)
{
    // does nothing

}

void CLIntroVideoitem::goToSteadyMode(bool steady, bool instant)
{
    if (steady)
        CLVideoWindowItem::goToSteadyMode(steady, instant);
    else
    {
        // one way ticket 
    }
}