#ifndef CAMERA_ADDITION_DIALOG_H
#define CAMERA_ADDITION_DIALOG_H

#include <QtCore/QEventLoop>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>

#include <api/model/manual_camera_seach_reply.h>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
    class CameraAdditionDialog;
}


class QnCheckBoxedHeaderView: public QHeaderView {
    Q_OBJECT
    typedef QHeaderView base_type;
public:
    explicit QnCheckBoxedHeaderView(QWidget *parent = 0);

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);

signals:
    void checkStateChanged(Qt::CheckState state);
protected:
    virtual void paintEvent(QPaintEvent *e) override;
    virtual void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    virtual QSize sectionSizeFromContents(int logicalIndex) const override;
private slots:
    void at_sectionClicked(int logicalIndex);
private:
    Qt::CheckState m_checkState;
};


class QnCameraAdditionDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT
    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    enum State {
        NoServer,           /**< No server is selected. */
        Initial,            /**< Ready to search cameras. */
        InitialOffline,     /**< Server is offline. */
        Searching,          /**< Search in progress. */
        Stopping,           /**< Stopping search. */
        CamerasFound,       /**< Some cameras found, ready to add. */
        CamerasOffline,     /**< Some cameras found but server went offline. */
        Adding,             /**< Adding in progress */

        Count
    };

    explicit QnCameraAdditionDialog(QWidget *parent = NULL);
    virtual ~QnCameraAdditionDialog();

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr &server);

    State state() const;

    virtual bool tryClose(bool force) override;
    virtual void reject() override;
private:
    Q_SLOT void clearTable();

    /** Update dialog ui elements corresponding to new state. */
    void setState(State state);

    /**
     * Fill table with the received cameras info.
     * \returns number of new cameras.
     */
    int fillTable(const QnManualCameraSearchCameraList &cameras);
    void removeAddedCameras();
    void updateSubnetMode();

    bool serverOnline() const;
    bool ensureServerOnline();
    bool addingAllowed() const;

    void updateTitle();
private slots: 
    void at_startIPLineEdit_textChanged(QString value);
    void at_startIPLineEdit_editingFinished();
    void at_endIPLineEdit_textChanged(QString value);
    void at_camerasTable_cellChanged(int row, int column);
    void at_camerasTable_cellClicked(int row, int column);
    void at_header_checkStateChanged(Qt::CheckState state);
    void at_scanButton_clicked();
    void at_stopScanButton_clicked();
    void at_addButton_clicked();
    void at_backToScanButton_clicked();
    void at_subnetCheckbox_toggled(bool toggled);
    void at_portAutoCheckBox_toggled(bool toggled);
    void at_server_statusChanged(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);

    void at_searchRequestReply(int status, const QVariant &reply, int handle);

    void updateStatus();
private:
    Q_DISABLE_COPY(QnCameraAdditionDialog)

    QScopedPointer<Ui::CameraAdditionDialog> ui;
    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;
    State m_state;
    QnMediaServerResourcePtr m_server;
    QnCheckBoxedHeaderView* m_header;

    bool m_inIpRangeEdit;
    bool m_subnetMode;
    bool m_inCheckStateChange;

    /** Uuid of the currently running process (if any). */
    QUuid m_processUuid;
};

#endif // CAMERA_ADDITION_DIALOG_H
