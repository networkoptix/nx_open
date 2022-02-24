// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

struct LayoutSettingsDialogState;

class NX_VMS_CLIENT_DESKTOP_API LayoutSettingsDialogStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit LayoutSettingsDialogStore(QObject* parent = nullptr);
    virtual ~LayoutSettingsDialogStore() override;

    const LayoutSettingsDialogState& state() const;

    // Actions.
    void loadLayout(const QnLayoutResourcePtr& layout);

    void setLocked(bool value);
    void setLogicalId(int value);
    void setOtherLogicalIds(const std::set<int>& value);
    void resetLogicalId();
    void generateLogicalId();
    void setFixedSizeEnabled(bool value);
    void setFixedSizeWidth(int value);
    void setFixedSizeHeight(int value);

    void setBackgroundImageError(const QString& errorText);
    void clearBackgroundImage();
    void setBackgroundImageOpacityPercent(int value);
    void setBackgroundImageWidth(int value);
    void setBackgroundImageHeight(int value);
    void setCropToMonitorAspectRatio(bool value);
    void setKeepImageAspectRatio(bool value);

    void startDownloading(const QString& targetPath);
    void imageDownloaded();
    void imageSelected(const QString& filename);
    void startUploading();
    void imageUploaded(const QString& filename);
    void setPreview(const QImage& image);

signals:
    void stateChanged(const LayoutSettingsDialogState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
