#include "video_wnd_archive_item.h"
#include "subitem/archive_navifation/archive_navigator_item.h"
#include "base/log.h"
#include "camera/camera.h"
#include "device_plugins/archive/abstract_archive_stream_reader.h"
#include "navigationitem.h"

CLVideoWindowArchiveItem::CLVideoWindowArchiveItem (GraphicsView* view, const CLDeviceVideoLayout* layout, 
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
