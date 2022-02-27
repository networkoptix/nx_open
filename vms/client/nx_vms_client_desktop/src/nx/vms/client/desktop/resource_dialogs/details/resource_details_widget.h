// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/common/widgets/panel.h>

namespace Ui { class ResourceDetailsWidget; }

namespace nx::vms::client::desktop {

class CameraThumbnailManager;

class ResourceDetailsWidget: public Panel
{
    Q_OBJECT
    using base_type = Panel;

public:
    explicit ResourceDetailsWidget(QWidget* parent = nullptr);
    virtual ~ResourceDetailsWidget() override;

    void clear();

    QnResourcePtr thumbnailCameraResource() const;
    void setThumbnailCameraResource(const QnResourcePtr& resource);

    QString captionText() const;
    void setCaptionText(const QString& text);

    QString descriptionText() const;
    void setDescriptionText(const QString& text);

    QString warningCaptionText() const;
    void setWarningCaptionText(const QString& text);

    QString warningExplanationText() const;
    void setWarningExplanationText(const QString& text);

private:
    const std::unique_ptr<Ui::ResourceDetailsWidget> ui;
    const std::unique_ptr<CameraThumbnailManager> m_cameraThumbnailManager;
};

} // namespace nx::vms::client::desktop
