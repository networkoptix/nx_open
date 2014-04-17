#ifndef WORKBENCH_BOOKMARKS_HANDLER_H
#define WORKBENCH_BOOKMARKS_HANDLER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

/**
 * @brief The QnWorkbenchBookmarksHandler class         Handler for camera bookmarks management.
 */
class QnWorkbenchBookmarksHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchBookmarksHandler(QObject *parent = NULL);
private slots:
    void at_bookmarkTimeSelectionAction_triggered();
};

#endif // WORKBENCH_BOOKMARKS_HANDLER_H
