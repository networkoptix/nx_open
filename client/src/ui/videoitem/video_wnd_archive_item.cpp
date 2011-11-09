#include "video_wnd_archive_item.h"
#include "subitem/archive_navifation/archive_navigator_item.h"
#include "camera/camera.h"
#include "navigationitem.h"

CLVideoWindowArchiveItem::CLVideoWindowArchiveItem (GraphicsView* view, const QnVideoResourceLayout* layout, 
                                                    int max_width, int max_height, const QString &name) :
CLVideoWindowItem(view, layout, max_width, max_height, name)
{
}

void CLVideoWindowArchiveItem::setItemSelected(bool sel, bool animate, int delay)
{
    CLVideoWindowItem::setItemSelected(sel, animate , delay );

    if (sel)
    {
        if (m_navigationItem)
            m_navigationItem->setVideoCamera(getVideoCam());
    }
    else
    {
        if (m_navigationItem)
            m_navigationItem->setVideoCamera(0);
    }
}
