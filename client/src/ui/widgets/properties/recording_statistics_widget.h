#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <api/model/recording_stats_reply.h>
#include <api/model/storage_space_reply.h>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnRecordingStatsModel;

namespace Ui {
    class RecordingStatisticsWidget;
}

class QnRecordingStatisticsWidget: public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnRecordingStatisticsWidget(const QnMediaServerResourcePtr &server, QWidget* parent = 0);
    virtual ~QnRecordingStatisticsWidget();

    virtual void updateFromSettings() override;
private:
    void updateData();

private slots:
    void at_gotStatiscits(int status, const QnRecordingStatsReply& data, int requestNum);
    void at_gotStorageSpace(int status, const QnStorageSpaceReply& data, int requestNum);
    void at_eventsGrid_clicked(const QModelIndex & index);
    void at_eventsGrid_customContextMenuRequested(const QPoint& screenPos);
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_mouseButtonRelease(QObject* sender, QEvent* event);
    void at_forecastParamsChanged();
    void at_updateColors();

private:
    void requestFinished();
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
    QScopedPointer<Ui::RecordingStatisticsWidget> ui;

    QnMediaServerResourcePtr m_server;

    QnRecordingStatsModel *m_model;
    QMap<int, QnUuid> m_requests;

    bool m_updateDisabled;
    bool m_dirty;

    QAction *m_selectAllAction;
    QAction *m_exportAction;
    QAction *m_clipboardAction;
    Qt::MouseButton m_lastMouseButton;
    QnRecordingStatsReply m_allData;
    QnRecordingStatsReply m_hiddenCameras;

    QVector<QnStorageSpaceData> m_availStorages;

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
