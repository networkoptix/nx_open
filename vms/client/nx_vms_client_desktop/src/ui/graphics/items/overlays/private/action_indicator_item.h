#pragma once

#include <chrono>

#include <QtWidgets/QGraphicsWidget>

#include <ui/customization/customized.h>

#include <nx/utils/impl_ptr.h>

class QnActionIndicatorItem: public Customized<QGraphicsWidget>
{
    Q_OBJECT
    using base_type = Customized<QGraphicsWidget>;

public:
    explicit QnActionIndicatorItem(QGraphicsWidget* parent = nullptr);
    virtual ~QnActionIndicatorItem() override = default;

    QString text() const;
    void setText(const QString& value);

    void clear(std::chrono::milliseconds after);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget = nullptr) override;

protected:
    virtual void changeEvent(QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
