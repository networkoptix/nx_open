#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/customization/customized.h>

class QModelIndex;

namespace Ui { class EventTile; }

namespace nx {
namespace client {
namespace desktop {

class EventTile: public Customized<QWidget>
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QColor titleColor READ titleColor WRITE setTitleColor)
    Q_PROPERTY(QString description READ description WRITE setDescription)
    Q_PROPERTY(QString timestamp READ timestamp WRITE setTimestamp)
    Q_PROPERTY(QPixmap icon READ icon WRITE setIcon)
    Q_PROPERTY(QPixmap image READ image WRITE setImage)

    using base_type = Customized<QWidget>;

public:
    explicit EventTile(QWidget* parent = nullptr);
    explicit EventTile(
        const QString& title,
        const QPixmap& icon,
        const QString& timestamp = QString(),
        const QString& description = QString(),
        QWidget* parent = nullptr);

    virtual ~EventTile() override;

    QString title() const;
    void setTitle(const QString& value);

    QColor titleColor() const;
    void setTitleColor(const QColor& value);

    QString description() const;
    void setDescription(const QString& value);

    QString timestamp() const;
    void setTimestamp(const QString& value);

    QPixmap icon() const;
    void setIcon(const QPixmap& value);

    QPixmap image() const;
    void setImage(const QPixmap& value);

    // Creates a tile widget for a data model item using the following roles:
    //     Qt::DisplayRole for title
    //     Qt::DecorationRole for icon
    //     Qn::TimestampTextRole for timestamp
    //     Qn::DescriptionTextRole for description
    //     Qt::ForegroundRole for titleColor
    static EventTile* createFrom(const QModelIndex& index, QWidget* parent = nullptr);

signals:
    // Links can be passed and displayed in description field.
    void linkActivated(const QString& link);

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    QScopedPointer<Ui::EventTile> ui;
};

} // namespace
} // namespace client
} // namespace nx
