#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QAction>
#include <QtWidgets/QWidget>

#include <ui/customization/customized.h>

#include <nx/client/desktop/common/utils/command_action.h>
#include <nx/utils/disconnect_helper.h>
#include <nx/utils/uuid.h>

class QModelIndex;
class QPushButton;
class QnImageProvider;

namespace Ui { class EventTile; }

namespace nx {
namespace client {
namespace desktop {

class EventTile: public Customized<QWidget>
{
    Q_OBJECT
    Q_PROPERTY(QnUuid id READ id)
    Q_PROPERTY(bool closeable READ closeable WRITE setCloseable)
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QColor titleColor READ titleColor WRITE setTitleColor)
    Q_PROPERTY(QString description READ description WRITE setDescription)
    Q_PROPERTY(QString timestamp READ timestamp WRITE setTimestamp)
    Q_PROPERTY(QPixmap icon READ icon WRITE setIcon)

    using base_type = Customized<QWidget>;

public:
    explicit EventTile(const QnUuid& id, QWidget* parent = nullptr);
    explicit EventTile(
        const QnUuid& id,
        const QString& title,
        const QPixmap& icon,
        const QString& timestamp = QString(),
        const QString& description = QString(),
        QWidget* parent = nullptr);

    virtual ~EventTile() override;

    QnUuid id() const;

    bool closeable() const;
    void setCloseable(bool value);

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

    QnImageProvider* preview() const;
    void setPreview(QnImageProvider* value);

    CommandActionPtr action() const;
    void setAction(const CommandActionPtr& value);

signals:
    void clicked();

    void closeRequested();

    // Links can be passed and displayed in description field.
    void linkActivated(const QString& link);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual bool event(QEvent* event) override;

private:
    void handleHoverChanged(bool hovered);

private:
    QScopedPointer<Ui::EventTile> ui;
    QPushButton* const m_closeButton = nullptr;
    QnUuid m_id;
    bool m_closeable = false;
    CommandActionPtr m_action; //< Button action.
};

} // namespace
} // namespace client
} // namespace nx
