#include "io_module_grid_overlay_contents.h"

#include <array>
#include <functional>

#include <ui/processors/clickable.h>
#include <utils/common/connective.h>


class QnIoModuleGridOverlayContentsPrivate: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

    Q_DISABLE_COPY(QnIoModuleGridOverlayContentsPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleGridOverlayContents)

    QnIoModuleGridOverlayContents* const q_ptr;

public:
    QnIoModuleGridOverlayContentsPrivate(QnIoModuleGridOverlayContents* main);

    void portsChanged(const QnIOPortDataList& ports, bool userInputEnabled);
    void stateChanged(const QnIOPortData& port, const QnIOStateData& state);

private:
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

        qreal effectiveIdWidth() const;

    protected:
        virtual void changeEvent(QEvent* event);

        void paintId(QPainter* painter, const QRectF& rect, bool on);
        void paintLabel(QPainter* painter, const QRectF& rect,
            Qt::AlignmentFlag horizontalAlignment);

    private:
        void setupFonts();
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
        mutable qreal m_elidedLabelWidth;
        mutable QString m_elidedLabel;
    };

    class InputPortItem: public PortItem
    {
        using base_type = PortItem;

    public:
        InputPortItem(const QString& id, QGraphicsItem* parent = nullptr);
        virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    };

    class OutputPortItem: public Clickable<PortItem>
    {
        using base_type = Clickable<PortItem>;

    public:
        /* We cannot use signal-slot mechanism in nested class. */
        using ClickHandler = std::function<void(const QString&)>;

        OutputPortItem(const QString& id, ClickHandler clickHandler, QGraphicsItem* parent = nullptr);
        virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    protected:
        virtual void clickedNotify(QGraphicsSceneMouseEvent* event) override;

    private:
        ClickHandler m_clickHandler;
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
            const QSizeF& constraint = QSizeF()) const override;

    private:
        using Position = QPoint;
        struct ItemWithPosition
        {
            PortItem* item;
            Position position;
        };

        using Items = QList<ItemWithPosition>;

        ItemWithPosition& itemWithPosition(int index);
        void ensureLayout();

        using Location = QPair<Items&, int>;
        Location locate(int index);

        using ConstLocation = QPair<const Items&, int>;
        ConstLocation locate(int index) const;

    private:
        Items m_inputItems;
        Items m_outputItems;
        QHash<QString, PortItem*> m_itemsById;

        enum Columns
        {
            kLeftColumn,
            kRightColumn,

            kColumnCount
        };

        std::array<int, kColumnCount> m_rowCounts;
        bool m_layoutDirty;
    };

private:
    PortItem* createItem(const QnIOPortData& port);
    QnIoModuleOverlayWidget* widget() const;

private:
    Layout* layout;
};
