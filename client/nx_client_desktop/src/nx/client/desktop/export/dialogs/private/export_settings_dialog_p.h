#pragma once

#include <array>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/export/data/export_media_settings.h>
#include <nx/client/desktop/export/data/export_layout_settings.h>
#include <nx/client/desktop/export/dialogs/export_settings_dialog.h>
#include <nx/client/desktop/export/widgets/export_overlay_widget.h>

#include <utils/common/connective.h>

class QWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class ExportSettingsDialog::Private: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    enum class ErrorCode
    {
        ok,
    };

    enum class OverlayType
    {
        timestamp,
        image,
        text,
        overlayCount
    };

    explicit Private(QObject* parent = nullptr);
    virtual ~Private() override;

    void loadSettings();

    void setMediaResource(const QnMediaResourcePtr& media);
    void setTimePeriod(const QnTimePeriod& period);
    void setFilename(const QString& filename);

    ErrorCode status() const;
    static bool isExportAllowed(ErrorCode code);

    ExportSettingsDialog::Mode mode();

    const ExportMediaSettings& exportMediaSettings() const;
    const ExportLayoutSettings& exportLayoutSettings() const;

    void setExportMediaSettings(const ExportMediaSettings& settings);

    void createOverlays(QWidget* overlayContainer);

    ExportOverlayWidget* overlay(OverlayType type);
    const ExportOverlayWidget* overlay(OverlayType type) const;

signals:
    void statusChanged(ErrorCode value);

private:
    void setStatus(ErrorCode value);
    void updateOverlays();

private:
    ExportSettingsDialog::Mode m_mode = Mode::Media;
    ErrorCode m_status = ErrorCode::ok;
    ExportMediaSettings m_exportMediaSettings;
    ExportLayoutSettings m_exportLayoutSettings;

    static constexpr size_t overlayCount = size_t(OverlayType::overlayCount);
    std::array<ExportOverlayWidget*, overlayCount> m_overlays {};
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx