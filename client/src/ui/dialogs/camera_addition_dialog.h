#ifndef CAMERA_ADDITION_DIALOG_H
#define CAMERA_ADDITION_DIALOG_H

#include <QtCore/QEventLoop>

#include <QtGui/QDialog>
#include <QtGui/QHeaderView>

#include <core/resource/resource_fwd.h>
#include <api/media_server_cameras_data.h>

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


class QnCameraAdditionDialog: public QDialog {
    Q_OBJECT
public:
    explicit QnCameraAdditionDialog(QWidget *parent = NULL);
    virtual ~QnCameraAdditionDialog();

    void setServer(const QnMediaServerResourcePtr &server);
signals:
    void serverChanged();

private:
    void clearTable();
    void fillTable(const QnCamerasFoundInfoList &cameras);
    void removeAddedCameras();
    void updateSubnetMode();
    bool ensureServerOnline();
private slots: 
    void at_startIPLineEdit_textChanged(QString value);
    void at_startIPLineEdit_editingFinished();
    void at_endIPLineEdit_textChanged(QString value);
    void at_camerasTable_cellChanged(int row, int column);
    void at_camerasTable_cellClicked(int row, int column);
    void at_header_checkStateChanged(Qt::CheckState state);
    void at_scanButton_clicked();
    void at_addButton_clicked();
    void at_subnetCheckbox_toggled(bool toggled);
    void at_resPool_resourceChanged(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);
private:
    Q_DISABLE_COPY(QnCameraAdditionDialog)

    QScopedPointer<Ui::CameraAdditionDialog> ui;
    QnMediaServerResourcePtr m_server;
    QnCheckBoxedHeaderView* m_header;

    bool m_inIpRangeEdit;
    bool m_subnetMode;
    bool m_inCheckStateChange;
};

#endif // CAMERA_ADDITION_DIALOG_H
