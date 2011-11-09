#ifndef video_wnd_archive_h1753
#define video_wnd_archive_h1753

#include "video_wnd_item.h"
class CLArchiveNavigatorItem;
class QnAbstractArchiveReader;

class CLVideoWindowArchiveItem : public CLVideoWindowItem
{
public:
    CLVideoWindowArchiveItem(GraphicsView* view, const QnVideoResourceLayout* layout, int max_width, int max_height,
                             const QString &name = QString());

    virtual void setItemSelected(bool sel, bool animate = true, int delay = 0);

    NavigationItem *navigationItem() const { return m_navigationItem; }
    void setNavigationItem(NavigationItem *item) { m_navigationItem = item; }

protected:
    NavigationItem *m_navigationItem;
};

#endif //video_wnd_archive_h1753
