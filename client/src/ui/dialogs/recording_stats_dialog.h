#ifndef QN_RECORDING_STATISTICS_DIALOG_H
#define QN_RECORDING_STATISTICS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include "api/model/recording_stats_reply.h"
#include "api/model/storage_space_reply.h"

class QnRecordingStatsModel;

namespace Ui {
    class RecordingStatsDialog;
}

class QnRecordingStatsItemDelegate: public QStyledItemDelegate 
{
    typedef QStyledItemDelegate base_type;

public:
    explicit QnRecordingStatsItemDelegate(QObject *parent = NULL): base_type(parent) {}
    virtual void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
};

class QnRecordingStatsDialog: public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnRecordingStatsDialog(QWidget *parent);
    virtual ~QnRecordingStatsDialog();

    void disableUpdateData();
    void enableUpdateData();
    void setServer(QnMediaServerResourcePtr mserver);
protected:
    void setVisible(bool value) override;

private slots:
    void updateData();
    void at_gotStatiscits(int httpStatus, const QnRecordingStatsReply& data, int requestNum);
    void at_gotStorageSpace(int httpStatus, const QnStorageSpaceReply& data, int requestNum);
    void at_eventsGrid_clicked(const QModelIndex & index);
    void at_eventsGrid_customContextMenuRequested(const QPoint& screenPos);
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_mouseButtonRelease(QObject* sender, QEvent* event);
    void at_forecastParamsChanged();
private:
    void requestFinished();
    QList<QnMediaServerResourcePtr> getServerList() const;
    QnRecordingStatsReply getForecastData(qint64 extraSizeBytes);

    /**
     * Get data from server
     * 
     * \param fromMsec start date. UTC msecs
     * \param toMsec end date. UTC msecs. Can be DATETIME_NOW
     */
    void query(qint64 bitrateAnalizePeriodMs);
    qint64 sliderPositionToBytes(int value) const;
    int bytesToSliderPosition (qint64 value) const;
    void updateColumnWidth();
private:
    QScopedPointer<Ui::RecordingStatsDialog> ui;
    QnRecordingStatsModel *m_model;

    QMap<int, QnUuid> m_requests;

    bool m_updateDisabled;
    bool m_dirty;

    QAction *m_selectAllAction;
    QAction *m_exportAction;
    QAction *m_clipboardAction;
    Qt::MouseButton m_lastMouseButton;
    QnRecordingStatsReply m_allData;
    QnRecordingStatsReply m_hidenCameras;

    QVector<QnStorageSpaceData> m_availStorages;
    QnMediaServerResourcePtr m_mserver;
private:
    // forecast related data
    struct ForecastDataPerCamera
    {
        ForecastDataPerCamera(): expand(false), maxDays(0), bytesPerStep(0), usageCoeff(0.0) {}
        
        QnCamRecordingStatsData stats;         // forecasted statistics
        bool expand;                           // do expand archive for that camera in the forecast
        int maxDays;                           // cached camera 'maxDays' value
        qint64 bytesPerStep;                   // how may bytes camera gives per calendar hour
        qreal usageCoeff;                      // how many time camera is actually recording in range [0..1]
    };

    struct ForecastData
    {
        ForecastData(): extraSpace(0) {}
        qint64 extraSpace; // extra space in bytes 
        QVector<ForecastDataPerCamera> cameras; // camera list by server
    };


private:
    QnRecordingStatsReply doForecastMainStep(ForecastData& forecastData, qint64 forecastStep);

};

#endif // QN_RECORDING_STATISTICS_DIALOG_H
