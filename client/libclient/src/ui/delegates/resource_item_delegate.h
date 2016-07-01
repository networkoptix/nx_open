#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QStyledItemDelegate>

#include <client/client_color_types.h>
#include <ui/customization/customized.h>

class QnWorkbench;

class QnResourceItemDelegate : public Customized<QStyledItemDelegate>
{
    Q_OBJECT
    typedef Customized<QStyledItemDelegate> base_type;

    Q_PROPERTY(QnResourceItemColors colors READ colors WRITE setColors)

public:
    explicit QnResourceItemDelegate(QObject* parent = nullptr);

    QnWorkbench* workbench() const;
    void setWorkbench(QnWorkbench* workbench);

    const QnResourceItemColors& colors() const;
    void setColors(const QnResourceItemColors& colors);

    /* If this value is non-zero, it is used as a fixed row height.
     * If zero, a minimal sufficient height is computed automatically. */
    int fixedHeight() const;
    void setFixedHeight(int height);

    /* Additional row spacing. */
    int rowSpacing() const;
    void setRowSpacing(int value);

    Qn::ResourceInfoLevel customInfoLevel() const;
    void setCustomInfoLevel(Qn::ResourceInfoLevel value);

protected:
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& styleOption, const QModelIndex& index) const;
    virtual QSize sizeHint(const QStyleOptionViewItem& styleOption, const QModelIndex& index) const override;

    virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const;

    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    virtual void destroyEditor(QWidget* editor, const QModelIndex& index) const override;

    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    enum class ItemState
    {
        Normal,
        Selected,
        Accented
    };

    ItemState itemState(const QModelIndex& index) const;

    void getDisplayInfo(const QModelIndex& index, QString& baseName, QString& extInfo) const;
private:
    QPointer<QnWorkbench> m_workbench;
    QIcon m_recordingIcon;
    QIcon m_scheduledIcon;
    QIcon m_buggyIcon;
    QnResourceItemColors m_colors;
    int m_fixedHeight;
    int m_rowSpacing;
    Qn::ResourceInfoLevel m_customInfoLevel;
};
