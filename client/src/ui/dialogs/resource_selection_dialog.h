#ifndef SELECT_CAMERAS_DIALOG_H
#define SELECT_CAMERAS_DIALOG_H

#include <QDialog>

#include <core/resource/resource_fwd.h>

#include <client/client_globals.h>

#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnResourceSelectionDialog;
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

class QnResourceSelectionDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QDialog base_type;

public:
    explicit QnResourceSelectionDialog(QWidget *parent, Qn::NodeType rootNodeType = Qn::ServersNode);
    ~QnResourceSelectionDialog();

    QnResourceList getSelectedResources() const;
    void setSelectedResources(const QnResourceList &selected);

    QnResourceSelectionDialogDelegate* delegate();
    void setDelegate(QnResourceSelectionDialogDelegate* delegate);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;

private slots:
    void at_resourceModel_dataChanged();

private:
    QScopedPointer<Ui::QnResourceSelectionDialog> ui;
    QnResourcePoolModel *m_resourceModel;
    QnResourceSelectionDialogDelegate* m_delegate;
    bool m_flat;
};

#endif // SELECT_CAMERAS_DIALOG_H
