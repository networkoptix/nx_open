#ifndef QN_RESOURCE_LIST_DIALOG_H
#define QN_RESOURCE_LIST_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>

#include "button_box_dialog.h"

class QTableWidget;
class QLabel;

class QnResourceListDialog: public QnButtonBoxDialog {
    Q_OBJECT;
public:
    QnResourceListDialog(QWidget *parent = NULL);
    virtual ~QnResourceListDialog();

    void setText(const QString &text);
    QString text() const;

    void setStandardButtons(QDialogButtonBox::StandardButtons standardButtons);
    QDialogButtonBox::StandardButtons standardButtons() const;

    const QnResourceList &resources() const;
    void setResources(const QnResourceList &resources);

private:
    QnResourceList m_resources;
    QLabel *m_label;
    QTableWidget *m_tableWidget;
    QDialogButtonBox *m_buttonBox;
};


#endif // QN_RESOURCE_LIST_DIALOG_H
