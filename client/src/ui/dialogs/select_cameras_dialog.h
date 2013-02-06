#ifndef SELECT_CAMERAS_DIALOG_H
#define SELECT_CAMERAS_DIALOG_H

#include <QDialog>

#include <core/resource/resource_fwd.h>
#include <core/resource_managment/resource_criterion.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnSelectCamerasDialog;
}

class QnResourcePoolModel;

// TODO: #GDM enable with QIdentityProxyModel in QT 4.8
/*class QnColoringProxyModel: public QIdentityProxyModel {
    Q_OBJECT

public:
    QnColoringProxyModel(QObject *parent = 0): QIdentityProxyModel(parent){}

    QVariant data(const QModelIndex &proxyIndex, int role) const override {
        if (role == Qt::TextColorRole)
            return QBrush(QColor(Qt::red));
            //todo: use delegate
        return QIdentityProxyModel::data(proxyIndex, role);
    }

};*/

class QnSelectCamerasDialogDelegate: public QObject
{
    Q_OBJECT
public:
    explicit QnSelectCamerasDialogDelegate(QObject* parent = NULL);
    ~QnSelectCamerasDialogDelegate();

    virtual void setWidgetLayout(QLayout* layout);
    virtual void modelDataChanged(const QnResourceList &selected);
    virtual bool isApplyAllowed();
};

// TODO: #GDM camera selection dialog 
class QnSelectCamerasDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QDialog base_type;

public:
    explicit QnSelectCamerasDialog(const QnResourceCriterion &criterion = QnResourceCriterion(), QWidget *parent = 0, QnWorkbenchContext *context = NULL);
    ~QnSelectCamerasDialog();

    QnResourceList getSelectedResources() const;
    void setSelectedResources(const QnResourceList &selected);

    QnSelectCamerasDialogDelegate* delegate();
    void setDelegate(QnSelectCamerasDialogDelegate* delegate);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;

private slots:
    void at_resourceModel_dataChanged();

private:
    QScopedPointer<Ui::QnSelectCamerasDialog> ui;
    QnResourcePoolModel *m_resourceModel;
    QnSelectCamerasDialogDelegate* m_delegate;
};

#endif // SELECT_CAMERAS_DIALOG_H
