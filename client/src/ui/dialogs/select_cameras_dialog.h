#ifndef SELECT_CAMERAS_DIALOG_H
#define SELECT_CAMERAS_DIALOG_H

#include <QDialog>

#include <core/resource/resource_fwd.h>

#include <client/client_globals.h>

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

    /**
     * @brief init                  This method allows delegate setup custom error or statistics widget
     *                              that will be displayed below the resources list.
     * @param parent                Parent container widget.
     */
    virtual void init(QWidget* parent);

    /**
     * @brief validate              This method allows delegate modify message depending on the selection
     *                              and control the OK button status.
     * @param selectedResources     List of selected resources.
     * @return                      True if selection is valid and OK button can be pressed.
     */
    virtual bool validate(const QnResourceList &selectedResources);
};

// TODO: #GDM rename to SelectResourcesDialog
class QnSelectCamerasDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QDialog base_type;

public:
    explicit QnSelectCamerasDialog(QWidget *parent, Qn::NodeType rootNodeType = Qn::ServersNode);
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
    bool m_flat;
};

#endif // SELECT_CAMERAS_DIALOG_H
