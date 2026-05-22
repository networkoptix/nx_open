// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <core/resource/camera_bookmark.h>

namespace nx::vms::client::desktop::workbench::timeline {

class BookmarkTooltip: public QQuickWidget
{
    Q_OBJECT

    using base_type = QQuickWidget;

public:
    explicit BookmarkTooltip(common::CameraBookmarkList bookmarks, QWidget* parent = nullptr);

    void setAllowExport(bool allowExport);
    void setAllowShare(bool allowShare);
    void setReadOnly(bool readOnly);

    Q_INVOKABLE void onPlayClicked(int index);
    Q_INVOKABLE void onEditClicked(int index);
    Q_INVOKABLE void onExportClicked(int index);
    Q_INVOKABLE void onShareClicked(int index);
    Q_INVOKABLE void onDeleteClicked(int index);

signals:
    void playClicked(const nx::vms::common::CameraBookmark& bookmark);
    void editClicked(const nx::vms::common::CameraBookmark& bookmark);
    void exportClicked(const nx::vms::common::CameraBookmark& bookmark);
    void shareClicked(const nx::vms::common::CameraBookmark& bookmark);
    void deleteClicked(const nx::vms::common::CameraBookmark& bookmark);
    void tagClicked(const QString& tag);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    common::CameraBookmarkList m_bookmarks;
};

} // namespace nx::vms::client::desktop::workbench::timeline
