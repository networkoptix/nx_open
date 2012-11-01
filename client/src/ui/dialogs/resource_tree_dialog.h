#ifndef RESOURCE_TREE_DIALOG_H
#define RESOURCE_TREE_DIALOG_H

#include <QDialog>

namespace Ui {
    class QnResourceTreeDialog;
}

class QnResourcePoolModel;

class QnResourceTreeDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit QnResourceTreeDialog(QWidget *parent = 0);
    ~QnResourceTreeDialog();
    
private:
    QScopedPointer<Ui::QnResourceTreeDialog> ui;

    QnResourcePoolModel *m_resourceModel;
};

#endif // RESOURCE_TREE_DIALOG_H
