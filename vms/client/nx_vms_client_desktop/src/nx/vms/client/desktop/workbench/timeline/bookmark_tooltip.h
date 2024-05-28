// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_bookmark_fwd.h>
#include <nx/vms/client/desktop/workbench/widgets/base_tooltip.h>

class QLayout;
class QPushButton;

namespace nx::vms::client::core { class ColorizedIconDeclaration; }

namespace nx::vms::client::desktop::workbench::timeline {

class BookmarkTooltip: public BaseTooltip
{
    Q_OBJECT

    using base_type = BaseTooltip;

public:
    BookmarkTooltip(
        const QnCameraBookmarkList& bookmarks,
        bool readOnly,
        QWidget* parent = nullptr);
    void setAllowExport(bool allowExport);

signals:
    void playClicked(const QnCameraBookmark& bookmark);
    void editClicked(const QnCameraBookmark& bookmark);
    void exportClicked(const QnCameraBookmark& bookmark);
    void deleteClicked(const QnCameraBookmark& bookmark);
    void tagClicked(const QString& tag);

protected:
    virtual void colorizeBorderShape(const QPainterPath& borderShape) override;

private:
    QWidget* createMoreItemsLabel();
    QWidget* createBookmarkSeparator();
    QWidget* createNameLabel(const QString& text);
    QWidget* createDescriptionLabel(const QString& text);
    QWidget* createTagButton(const QString& tag);
    QWidget* createInternalSeparator();
    QPushButton* createButton(const core::ColorizedIconDeclaration& iconName, const QString& toolTip);
    QLayout* createButtonsLayout(const QnCameraBookmark& bookmark);

private:
    bool m_readOnly = false;
    QPushButton* m_exportButton = nullptr;
};

} // namespace nx::vms::client::desktop::workbench::timeline
