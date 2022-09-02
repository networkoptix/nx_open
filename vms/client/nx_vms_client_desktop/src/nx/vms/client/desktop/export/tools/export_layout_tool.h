// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/export/data/export_layout_settings.h>
#include <nx/vms/client/desktop/export/tools/abstract_export_tool.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <recording/stream_recorder.h>

class QnClientVideoCamera;

namespace nx::vms::client::desktop {

/**
 * Utility class for exporting multi-video layouts. New instance of the class should be created for
 * every new export process. Correct behaviour while re-using is not guarantied. Notifies about
 * progress of the process.
 */
class ExportLayoutTool: public AbstractExportTool
{
    Q_OBJECT
    using base_type = AbstractExportTool;

public:
    explicit ExportLayoutTool(
        ExportLayoutSettings settings,
        LayoutResourcePtr layout,
        QObject* parent = nullptr);
    virtual ~ExportLayoutTool() override;

    virtual bool start() override;
    virtual void stop() override;

    /**
     * @brief mode                              Current export mode.
     * @return
     */
    ExportLayoutSettings::Mode mode() const;

    virtual ExportProcessError lastError() const override;
    virtual ExportProcessStatus processStatus() const override;

private slots:
    bool exportMediaResource(const QnMediaResourcePtr& resource);

    void at_camera_progressChanged(int progress);
    void at_camera_exportFinished(const std::optional<nx::recording::Error>& status,
        const QString& filename);

private:
    /** Info about layout items */
    struct ItemInfo {
        QString name;
        qint64 timezone;

        ItemInfo();
        ItemInfo(const QString& name, qint64 timezone);
    };
    typedef QList<ItemInfo> ItemInfoList;

    /** Create and setup storage resource. */
    bool prepareStorage();

    /** Create and setup layout resource. */
    ItemInfoList prepareLayout();

    /** Write metadata to storage file. */
    bool exportMetadata(const ItemInfoList &items);

    bool exportNextCamera();
    void finishExport(bool success);

    bool tryInLoop(std::function<bool()> handler);
    bool writeData(const QString &fileName, const QByteArray &data);

private:
    struct Private;
    QScopedPointer<Private> d;

    /** Stage field, used to show correct progress in multi-video layout. */
    int m_offset = -1;

    QScopedPointer<QnClientVideoCamera> m_currentCamera;

    bool m_isExportToExe = false;
};

} // namespace nx::vms::client::desktop
