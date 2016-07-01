
#pragma once

#include <QtCore/QObject>

#include <client/client_color_types.h>
#include <utils/common/connective.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/customization/customized.h>

class QnMediaResourceWidget;

// @brief Loads bookmarks for all items on current layout in vicinity of current position.
class QnWorkbenchItemBookmarksWatcher : public Customized< Connective<QObject> >
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    Q_PROPERTY(QnBookmarkColors bookmarkColors READ bookmarkColors WRITE setBookmarkColors)

    typedef Customized< Connective<QObject> > base_type;

public:
    QnWorkbenchItemBookmarksWatcher(QObject *parent);

    virtual ~QnWorkbenchItemBookmarksWatcher();

    QnBookmarkColors bookmarkColors() const;

    void setBookmarkColors(const QnBookmarkColors &colors);

private:
    class WidgetData;
    typedef QSharedPointer<WidgetData> WidgetDataPtr;
    typedef QHash<QnMediaResourceWidget *, WidgetDataPtr> WidgetDataHash;

private:
    QnBookmarkColors m_colors;
    WidgetDataHash m_widgetDataHash;
};

