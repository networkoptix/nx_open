#pragma once

#include <chrono>

#include <client_core/connection_context_aware.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/watermark_settings.h>

namespace Ui { class SecuritySettingsWidget; }

namespace nx::vms::client::desktop {

class SecuritySettingsWidget: public QnAbstractPreferencesWidget, public QnConnectionContextAware
{
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    SecuritySettingsWidget(QWidget* parent = nullptr);
    virtual ~SecuritySettingsWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;

signals:
    void forceVideoTrafficEncryptionChanged(bool value);

private:
    std::chrono::minutes calculateSessionTimeout() const;
    void at_forceTrafficEncryptionCheckBoxClicked(bool value);

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    QScopedPointer<Ui::SecuritySettingsWidget> ui;
    QnWatermarkSettings m_watermarkSettings;
};

} // namespace nx::vms::client::desktop
