#pragma once

#include "io_module_overlay_widget.h"

#include <functional>

#include <QtWidgets/QGraphicsLayout>

#include <ui/processors/clickable.h>
#include <utils/common/connective.h>


class QnIoModuleOverlayContentsPrivate: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

    Q_DISABLE_COPY(QnIoModuleOverlayContentsPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleOverlayContents)

    QnIoModuleOverlayContents* const q_ptr;

protected:
    class PortItem: public GraphicsWidget
    {
        using base_type = GraphicsWidget;

    public:
        PortItem(const QString& id, bool isOutput, QGraphicsItem* parent = nullptr);

        QString id() const;
        bool isOutput() const;

        QString label() const;
        void setLabel(const QString& label);

        bool isOn() const;
        void setOn(bool on);

        qreal idWidth() const;

        qreal minIdWidth() const;
        void setMinIdWidth(qreal value);

    protected:
        qreal labelWidth() const;
        qreal effectiveIdWidth() const;

        virtual void changeEvent(QEvent* event);
        virtual void paint(
            QPainter* painter,
            const QStyleOptionGraphicsItem* option,
            QWidget* widget);

        virtual void setupFonts(QFont& idFont, QFont& activeIdFont, QFont& labelFont) = 0;
        virtual void paint(QPainter* painter) = 0;

        void paintId(QPainter* painter, const QRectF& rect, bool on);
        void paintLabel(QPainter* painter, const QRectF& rect,
            Qt::AlignmentFlag horizontalAlignment);

    private:
        void setTransparentForMouse(bool transparent);
        void ensureElidedLabel(qreal width) const;

    private:
        const QString m_id;
        const bool m_isOutput;
        QString m_label;
        bool m_on;
        qreal m_minIdWidth;

        QFont m_idFont;
        QFont m_activeIdFont;
        QFont m_labelFont;

        /* Cached values: */
        mutable qreal m_idWidth;
        mutable qreal m_labelWidth;
        mutable qreal m_elidedLabelWidth;
        mutable QString m_elidedLabel;
    };

    class InputPortItem: public PortItem
    {
        using base_type = PortItem;

    public:
        InputPortItem(const QString& id, QGraphicsItem* parent = nullptr);
    };

    class OutputPortItem: public Clickable<PortItem>
    {
        using base_type = Clickable<PortItem>;

    public:
        /* We cannot use signal-slot mechanism in nested class. */
        using ClickHandler = std::function<void(const QString&)>;
        OutputPortItem(const QString& id, ClickHandler clickHandler, QGraphicsItem* parent = nullptr);

    protected:
        bool isHovered() const;
        virtual QRectF activeRect() const = 0;

        virtual QPainterPath shape() const override;
        virtual bool sceneEvent(QEvent* event) override;
        virtual void clickedNotify(QGraphicsSceneMouseEvent* event) override;

    private:
        ClickHandler m_clickHandler;
        bool m_hovered;
    };

    class Layout: public QGraphicsLayout
    {
        using base_type = QGraphicsLayout;

    public:
        Layout(QGraphicsLayoutItem* parent = nullptr);

        void clear();
        bool addItem(PortItem* item);
        PortItem* itemById(const QString& id);

        virtual int count() const override;
        virtual QGraphicsLayoutItem* itemAt(int index) const override;
        virtual void removeAt(int index) override;

        virtual void setGeometry(const QRectF& rect) override;
        virtual void invalidate() override;

    protected:
        virtual QSizeF sizeHint(Qt::SizeHint which,
            const QSizeF& constraint = QSizeF()) const override = 0;

        virtual void recalculateLayout() = 0;

        using Position = QPoint;
        struct ItemWithPosition
        {
            PortItem* item;
            Position position;
        };

        ItemWithPosition& itemWithPosition(int index);

    private:
        using Items = QList<ItemWithPosition>;
        using Location = QPair<Items&, int>;
        Location locate(int index);
        using ConstLocation = QPair<const Items&, int>;
        ConstLocation locate(int index) const;

        void ensureLayout();

    protected:
        Items m_inputItems;
        Items m_outputItems;

    private:
        QHash<QString, PortItem*> m_itemsById;
        bool m_layoutDirty;
    };

public:
    QnIoModuleOverlayContentsPrivate(QnIoModuleOverlayContents* main, Layout* layout);

    void portsChanged(const QnIOPortDataList& ports, bool userInputEnabled);
    void stateChanged(const QnIOPortData& port, const QnIOStateData& state);

    Layout* layout() const;

protected:
    virtual PortItem* createItem(const QString& id, bool isOutput) = 0;

private:
    PortItem* createItem(const QnIOPortData& port);

private:
    Layout* m_layout;
};
