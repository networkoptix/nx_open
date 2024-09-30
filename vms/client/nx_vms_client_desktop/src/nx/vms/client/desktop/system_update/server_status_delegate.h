// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory> //< for shared_ptr
#include <vector>

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {

class LoadingIndicator;
struct UpdateItem;

// Fancy delegate to show status of updating server.
// It creates a complex widget to display state of each server.
class ServerStatusItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT

    using base_type = QStyledItemDelegate;

public:
    explicit ServerStatusItemDelegate(QWidget* parent = 0);
    ~ServerStatusItemDelegate();

    enum class StatusMode
    {
        hidden,
        remoteStatus,
        reportErrors,
    };

    QPixmap getCurrentAnimationFrame() const;
    void setStatusMode(StatusMode mode);
    bool isStatusVisible() const;
    bool isVerificationErrorVisible() const;

protected:
    class ServerStatusWidget;

    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;

private:
    QScopedPointer<LoadingIndicator> m_updateAnimation;
    StatusMode m_statusMode = StatusMode::hidden;
};

} // namespace nx::vms::client::desktop
