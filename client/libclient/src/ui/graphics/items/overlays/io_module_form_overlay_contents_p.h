#include "io_module_form_overlay_contents.h"
#include "io_module_overlay_contents_p.h"


class QnIoModuleFormOverlayContentsPrivate: public QnIoModuleOverlayContentsPrivate
{
    Q_OBJECT
    using base_type = QnIoModuleOverlayContentsPrivate;

    Q_DISABLE_COPY(QnIoModuleFormOverlayContentsPrivate)
    Q_DECLARE_PUBLIC(QnIoModuleFormOverlayContents)

    QnIoModuleFormOverlayContents* const q_ptr;

public:
    QnIoModuleFormOverlayContentsPrivate(QnIoModuleFormOverlayContents* main);

protected:
    virtual PortItem* createItem(const QString& id, bool isOutput);

private:
    /* Input port item implementation: */
    class InputPortItem: public base_type::InputPortItem
    {
        using base_type = QnIoModuleOverlayContentsPrivate::InputPortItem;

    public:
        using base_type::InputPortItem; //< forward constructors

    protected:
        virtual void paint(QPainter* painter) override;
        virtual void setupFonts(QFont& idFont, QFont& activeIdFont, QFont& labelFont) override;

        virtual QSizeF sizeHint(Qt::SizeHint which,
            const QSizeF& constraint = QSizeF()) const override;
    };

    /* Output port item implementation: */
    class OutputPortItem: public base_type::OutputPortItem
    {
        using base_type = QnIoModuleOverlayContentsPrivate::OutputPortItem;

    public:
        using base_type::OutputPortItem; //< forward constructors

    protected:
        virtual QRectF activeRect() const override;
        virtual void paint(QPainter* painter) override;
        virtual void setupFonts(QFont& idFont, QFont& activeIdFont, QFont& labelFont) override;

        virtual QSizeF sizeHint(Qt::SizeHint which,
            const QSizeF& constraint = QSizeF()) const override;

    private:
        void ensurePaths(const QRectF& buttonRect);

    private:
        /* Cached paint information: */
        QRectF m_lastButtonRect;
        QPainterPath m_buttonPath;
        QPainterPath m_topShadowPath;
        QPainterPath m_bottomShadowPath;
    };

    /* Layout implementation: */
    class Layout: public base_type::Layout
    {
        using base_type = QnIoModuleOverlayContentsPrivate::Layout;

    public:
        using base_type::Layout; //< forward constructors

        virtual void setGeometry(const QRectF& rect) override;

    protected:
        virtual QSizeF sizeHint(Qt::SizeHint which,
            const QSizeF& constraint = QSizeF()) const override;

        virtual void recalculateLayout() override;

    private:
        enum Columns
        {
            kLeftColumn,
            kRightColumn,

            kColumnCount
        };

        std::array<int, kColumnCount> m_rowCounts { 0 };
        std::array<qreal, kColumnCount> m_widthHints { 0.0 };
    };
};
