// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/skin/color_theme.h>
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

    void setThumbnailCameraResource(const QnResourcePtr& resource);

    /*
     * Sets text that will be displayed below thumbnail, if one will be acquired, and above other
     * text blocks in any case. Caption is displayed in bright accented style.
     */
    void setCaption(const QString& text);

    /*
     * Sets text of information message, optionally in user defined color.
     */
    void setMessage(const QString& text,
        const QColor& color = nx::vms::client::core::colorTheme()->color("light10"));

    /*
     * Removes information message, equivalent of setMessage({}) call.
     */
    void clearMessage();

    /*
     * Adds warning message that consists of caption and more detailed explanation displayed as
     * special warning block with exclamation mark icon and texts in a bright yellow color.
     */
    void addWarningMessage(const QString& warningCaption, const QString& warningMessage);

    /*
     * Removes all warning messages.
     */
    void clearWarningMessages();

private:
    const std::unique_ptr<Ui::ResourceDetailsWidget> ui;
    const std::unique_ptr<CameraThumbnailManager> m_cameraThumbnailManager;
};

} // namespace nx::vms::client::desktop
