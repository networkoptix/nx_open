#include "search_bookmarks_dialog.h"

#include <ui/dialogs/private/search_bookmarks_dialog_p.h>

QnSearchBookmarksDialog::QnSearchBookmarksDialog(const QString &filterText
    , qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs
    , QWidget *parent)
    : base_type(parent)
    , d_ptr(new QnSearchBookmarksDialogPrivate(filterText, utcStartTimeMs, utcFinishTimeMs, this))
{
}

QnSearchBookmarksDialog::~QnSearchBookmarksDialog()
{
}

void QnSearchBookmarksDialog::setParameters(qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs
    , const QString &filterText)
{
    Q_D(QnSearchBookmarksDialog);
    d->setParameters(filterText, utcStartTimeMs, utcFinishTimeMs);
}


void QnSearchBookmarksDialog::resizeEvent(QResizeEvent *event)
{
    base_type::resizeEvent(event);

    Q_D(QnSearchBookmarksDialog);
    d->updateHeadersWidth();
}

void QnSearchBookmarksDialog::showEvent(QShowEvent *event)
{
    base_type::showEvent(event);

    Q_D(QnSearchBookmarksDialog);
    d->refresh();
}
