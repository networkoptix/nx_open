#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

class QnSearchBookmarksDialogPrivate;

class QnSearchBookmarksDialog: public QnSessionAwareDialog
{
    using base_type = QnSessionAwareDialog;
public:
    QnSearchBookmarksDialog(const QString& filterText, qint64 utcStartTimeMs,
        qint64 utcFinishTimeMs, QWidget* parent);

    virtual ~QnSearchBookmarksDialog();

    void setParameters(qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs
        , const QString &filterText = QString());

private:
    void showEvent(QShowEvent *event);
    void closeEvent(QCloseEvent* event);

private:
    Q_DECLARE_PRIVATE(QnSearchBookmarksDialog)
    QScopedPointer<QnSearchBookmarksDialogPrivate> d_ptr;
};
