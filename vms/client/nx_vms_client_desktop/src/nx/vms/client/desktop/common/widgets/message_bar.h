// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/property_storage/storage.h>

#include "control_bars.h"

class QHBoxLayout;

namespace nx::vms::client::desktop {

struct BarDescription
{
    using PropertyPointer = QPointer<nx::utils::property_storage::Property<bool>>;
    enum class BarLevel
    {
        Info,
        Warning,
        Error
    };

    QString text;
    BarLevel level = BarLevel::Info;
    bool isMultiLine = false;
    bool isClosable = false;
    bool isOpenExternalLinks = true;
    PropertyPointer isEnabledProperty;
    std::vector<QHBoxLayout*> additionalRows; //<= Additional rows under the label.
};

/**
 * A text message on a colored background.
 * It's intended to be positioned in an external layout at a top or a bottom of a dialog.
 * Has appearance depending on type CommonMessageBar::BarLevel
 *
 * TODO: This is replacement of class MessageBar. Need to replace usage class MessageBar
 * and it's children to this new one, and then delete them. And change implementation
 * of AlertBlock to usage of CommonMessageBox
 */
class CommonMessageBar: public ControlBar
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    using base_type = ControlBar;

public:
    CommonMessageBar(QWidget* parent = nullptr, const BarDescription& description = {});
    virtual ~CommonMessageBar() override;

    QString text() const;
    void setText(const QString& text);

    void init(const BarDescription& barDescription);
    void updateVisibility();
    void hideInFuture();

    /** Enabled by default. */
    void setOpenExternalLinks(bool open);

signals:
    void closeClicked();
    /** Emitted only when `setOpenExternalLinks` is disabled. */
    void linkActivated(const QString& link);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
