// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/core/common/utils/common_module_aware.h>

namespace Ui { class LayoutBackgroundSettingsWidget; }

namespace nx::vms::client::desktop {

struct LayoutSettingsDialogState;
class LayoutSettingsDialogStore;

class LayoutBackgroundSettingsWidget:
    public QWidget,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit LayoutBackgroundSettingsWidget(
        LayoutSettingsDialogStore* store,
        QWidget* parent = nullptr);
    virtual ~LayoutBackgroundSettingsWidget() override;

    void uploadImage();

signals:
    void newImageUploadedSuccessfully();

protected:
    virtual bool eventFilter(QObject* target, QEvent* event) override;

private:
    void setupUi();
    void initCache(bool isLocalFile);
    void loadState(const LayoutSettingsDialogState& state);

private:
    void at_imageLoaded(const QString& filename, bool ok);
    void at_imageStored(const QString& filename, bool ok);

    void setPreview(const QImage& image);

    void viewFile();

    void selectFile();

    void loadPreview();

    void startDownloading();

private:
    struct Private;
    QScopedPointer<Private> d;

    QScopedPointer<Ui::LayoutBackgroundSettingsWidget> ui;
    const QPointer<LayoutSettingsDialogStore> m_store;
};

} // namespace nx::vms::client::desktop
