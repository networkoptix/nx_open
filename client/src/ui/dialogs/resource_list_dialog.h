#ifndef QN_RESOURCE_LIST_DIALOG_H
#define QN_RESOURCE_LIST_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>

#include "button_box_dialog.h"

class QnResourceListModel;

namespace Ui {
    class ResourceListDialog;
}

class QnResourceListDialog: public QnButtonBoxDialog {
    Q_OBJECT;
public:
    QnResourceListDialog(QWidget *parent = NULL);
    virtual ~QnResourceListDialog();

    void setText(const QString &text);
    QString text() const;

    void setBottomText(const QString &bottomText);
    QString bottomText() const;

    void setStandardButtons(QDialogButtonBox::StandardButtons standardButtons);
    QDialogButtonBox::StandardButtons standardButtons() const;

    const QnResourceList &resources() const;
    void setResources(const QnResourceList &resources);

    using QnButtonBoxDialog::exec;

    static QDialogButtonBox::StandardButton exec(QWidget *parent, const QnResourceList &resources, const QString &title, const QString &text, const QString &bottomText, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    static QDialogButtonBox::StandardButton exec(QWidget *parent, const QnResourceList &resources, const QString &title, const QString &text, QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

private:
    QScopedPointer<Ui::ResourceListDialog> ui;
    QnResourceListModel *m_model;
};


#endif // QN_RESOURCE_LIST_DIALOG_H
