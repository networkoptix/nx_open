#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QtGui/QDialog>
#include "connectinfo.h"
#include <utils/settings.h>
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include <utils/network/networkoptixmodulefinder.h>
#include <utils/network/foundenterprisecontrollersmodel.h>

class QDataWidgetMapper;
class QStandardItemModel;
class QUrl;

class QnWorkbenchContext;
class QnAbstractArchiveReader;
class QnVideoCamera;
class QnResourceWidgetRenderer;
class QnRenderingWidget;

namespace Ui {
    class LoginDialog;
    class FindAppServerDialog;
}

class QnCombineConnectionsModel : public QAbstractListModel {
    Q_OBJECT

public:
    QnCombineConnectionsModel(QObject* parent = 0): QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const {
        int count = 0;
        foreach (QAbstractItemModel *child, m_children)
            count += child->rowCount(parent);
        return count;
    }

    QVariant data(const QModelIndex &index, int role) const {
        int row = index.row();
        int count = 0;
        foreach (QAbstractItemModel *child, m_children) {
            if (count + child->rowCount() < row)
                count += child->rowCount();
            else
                return child->data(child->index(row - count, index.column()), role);
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation,
                             int role = Qt::DisplayRole) const {
        return QVariant();
    }

    void addChild(QAbstractItemModel *child) {
        m_children.append(child);
    }

private:
    QList<QAbstractItemModel*> m_children;
};

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    virtual ~LoginDialog();

    QUrl currentUrl() const;
    QString currentName() const;
    QnConnectInfoPtr currentInfo() const;

public slots:
    virtual void accept() override;
    virtual void reject() override;

protected:
    virtual void changeEvent(QEvent *event) override;

    /**
     * Reset connections model to its initial state. Select last used connection.
     */
    void resetConnectionsModel();

private slots:
    void updateAcceptibility();
    void updateFocus();
    void updateUsability();

    void at_testButton_clicked();
    void at_saveButton_clicked();
    void at_deleteButton_clicked();
    void at_connectionsComboBox_currentIndexChanged(int index);
    void at_connectFinished(int status, const QByteArray &errorString, QnConnectInfoPtr connectInfo, int requestHandle);
    void onSelectEntCtrlButtonClicked();

private:
    Q_DISABLE_COPY(LoginDialog)

    QScopedPointer<Ui::LoginDialog> ui;
    QScopedPointer<QDialog> m_findAppServerDialog;
    QScopedPointer<Ui::FindAppServerDialog> m_findAppServerDialogUI;
    QWeakPointer<QnWorkbenchContext> m_context;
    QStandardItemModel *m_connectionsModel;
    QDataWidgetMapper *m_dataWidgetMapper;
    int m_requestHandle;
    QnConnectInfoPtr m_connectInfo;

    QnRenderingWidget* m_renderingWidget;
    NetworkOptixModuleFinder m_entCtrlFinder;
    FoundEnterpriseControllersModel m_foundEntCtrlModel;

    QnCombineConnectionsModel* m_combineConnectionsModel;
};

#endif // LOGINDIALOG_H
