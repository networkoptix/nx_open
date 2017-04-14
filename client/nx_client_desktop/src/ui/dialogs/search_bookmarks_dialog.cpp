#include "search_bookmarks_dialog.h"

#include <ui/dialogs/private/search_bookmarks_dialog_p.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnSearchBookmarksDialog::QnSearchBookmarksDialog(
    const QString& filterText,
    qint64 utcStartTimeMs,
    qint64 utcFinishTimeMs,
    QWidget* parent)
    :
    base_type(parent),
    d_ptr(new QnSearchBookmarksDialogPrivate(filterText, utcStartTimeMs, utcFinishTimeMs, this))
{
    setHelpTopic(this, Qn::Bookmarks_Search_Help);

    setWindowFlags(windowFlags()
        | Qt::WindowMaximizeButtonHint
        | Qt::MaximizeUsingFullscreenGeometryHint);
}

QnSearchBookmarksDialog::~QnSearchBookmarksDialog()
{
}

void QnSearchBookmarksDialog::setParameters(qint64 utcStartTimeMs, qint64 utcFinishTimeMs,
    const QString &filterText)
{
    Q_D(QnSearchBookmarksDialog);
    d->setParameters(filterText, utcStartTimeMs, utcFinishTimeMs);
}

void QnSearchBookmarksDialog::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);

    Q_D(QnSearchBookmarksDialog);
    d->refresh();
}
