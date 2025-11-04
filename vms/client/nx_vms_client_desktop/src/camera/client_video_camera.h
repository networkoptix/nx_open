// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>
#include <nx/media/config.h>
#include <nx/vms/client/desktop/export/tools/nov_media_export.h>
#include <recording/time_period_list.h>

#include "cam_display.h"

class QnlTimeSource;
class QnMediaStreamStatistics;
class QnAbstractArchiveStreamReader;

// TODO: #amalov Used for export only. Remove.
class [[deprecated]] QnClientVideoCamera : public QObject
{
    Q_OBJECT

    Q_ENUMS(ClientVideoCameraError)

    typedef QObject base_type;
public:
    QnClientVideoCamera(const QnMediaResourcePtr &resource);

    virtual ~QnClientVideoCamera();

    QnMediaResourcePtr resource();

    // this function must be called if stream was interrupted or so; to sync audio and video again
    //void streamJump(qint64 time);

    void setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val);

    virtual QnResourcePtr getDevice() const;

    QnAbstractStreamDataProvider* getStreamreader();

    const QnMediaStreamStatistics* getStatistics(int channel = 0);
    QnCamDisplay* getCamDisplay();

    /*
    * Export motion stream to separate file
    */
    void setMotionIODevice(QSharedPointer<QBuffer>, int channel);
    QSharedPointer<QBuffer> motionIODevice(int channel);

    nx::vms::client::desktop::NovMediaExport* recorder() {return m_exportRecorder; }

    // TODO: #sivanov Refactor parameter set to a structure.
    void exportMediaPeriodToFile(const QnTimePeriod &timePeriod,
        const QString& fileName,
        QnStorageResourcePtr storage,
        const QTimeZone& timeZone,
        const QnTimePeriodList& playbackMask = QnTimePeriodList());

    void setResource(QnMediaResourcePtr resource);
    bool isDisplayStarted() const { return m_displayStarted; }
signals:
    void exportProgress(int progress);
    void exportFinished(const std::optional<nx::recording::Error>& reason, const QString &fileName);

public slots:
    virtual void startDisplay();
    virtual void beforeStopDisplay();
    virtual void stopDisplay();

    void stopExport();
private:
    mutable nx::Mutex m_exportMutex;
    QnMediaResourcePtr m_resource;
    QnCamDisplay m_camdispay; //< TODO: #sivanov Refactor to a scoped pointer.

    // TODO: #sivanov Refactor to unique pointer or 'owner' template.
    QPointer<QnAbstractMediaStreamDataProvider> m_reader;

    QPointer<nx::vms::client::desktop::NovMediaExport> m_exportRecorder;
    QPointer<QnAbstractMediaStreamDataProvider> m_exportReader;
    QSharedPointer<QBuffer> m_motionFileList[CL_MAX_CHANNELS];
    bool m_displayStarted;
};
