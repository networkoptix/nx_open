// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QStyledItemDelegate>

#include <common/common_globals.h>
#include <ui/common/text_pixmap_cache.h>

namespace nx::vms::client::desktop { class Workbench; }

/**
 * Delegate for displaying resources in resource tree.
 * It uses the following roles from the model:
 *  - Qt::DisplayRole base name for resource
 *  - core::ResourceRole resource pointer, QnResourcePtr
 *  - Qn::NodeTypeRole type of the node in resource tree, ResourceTree::NodeType
 */
class QnResourceItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    typedef QStyledItemDelegate base_type;

public:
    enum Option
    {
        NoOptions           = 0x0,
        RecordingIcons      = 0x1,
        LockedIcons         = 0x2,
        HighlightChecked    = 0x4,
        ValidateOnlyChecked = 0x8
    };
    Q_DECLARE_FLAGS(Options, Option)

    explicit QnResourceItemDelegate(QObject* parent = nullptr);

    nx::vms::client::desktop::Workbench* workbench() const;
    void setWorkbench(nx::vms::client::desktop::Workbench* workbench);

    /* If this value is non-zero, it is used as a fixed row height.
     * If zero, a minimal sufficient height is computed automatically. */
    int fixedHeight() const;
    void setFixedHeight(int height);

    /* Additional row spacing. */
    int rowSpacing() const;
    void setRowSpacing(int value);

    Qn::ResourceInfoLevel customInfoLevel() const;
    void setCustomInfoLevel(Qn::ResourceInfoLevel value);

    Options options() const;
    void setOptions(Options value);

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

    virtual QSize sizeHint(const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

    int checkBoxColumn() const;
    void setCheckBoxColumn(int value);

protected:
    static const QStyle::StateFlag State_Error
        = QStyle::State_AutoRaise; //< Use unused in item views value.

    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;
    virtual void destroyEditor(QWidget* editor, const QModelIndex& index) const override;

    enum class ItemState
    {
        normal,
        selected,
        accented
    };

    virtual ItemState itemState(const QModelIndex& index) const;
    virtual void getDisplayInfo(const QModelIndex& index,
        QString& baseName, QString& extInfo) const;

private:
    ItemState itemStateForMediaResource(const QModelIndex& index) const;
    ItemState itemStateForLayout(const QModelIndex& index) const;
    ItemState itemStateForRecorder(const QModelIndex& index) const;
    ItemState itemStateForLayoutItem(const QModelIndex& index) const;
    ItemState itemStateForVideoWall(const QModelIndex& index) const;
    ItemState itemStateForVideoWallItem(const QModelIndex& index) const;
    ItemState itemStateForShowreel(const QModelIndex& index) const;

    QVariant rowCheckState(const QModelIndex& index) const;

    void paintExtraStatus(QPainter* painter, const QRect& iconRect, const QModelIndex& index) const;

private:
    QPointer<nx::vms::client::desktop::Workbench> m_workbench;
    QIcon m_recordingIcon;
    QIcon m_scheduledIcon;
    QIcon m_lockedIcon;
    int m_fixedHeight;
    int m_rowSpacing;
    Qn::ResourceInfoLevel m_customInfoLevel;
    Options m_options;
    int m_checkBoxColumn = -1;
    mutable QnTextPixmapCache m_textPixmapCache;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceItemDelegate::Options)
