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
class QnElidedLabel;
class QnImageProvider;

namespace Ui { class EventTile; }

namespace nx {
namespace client {
namespace desktop {

class EventTile: public Customized<QWidget>
{
    Q_OBJECT
    Q_PROPERTY(bool closeable READ closeable WRITE setCloseable)
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QColor titleColor READ titleColor WRITE setTitleColor)
    Q_PROPERTY(QString description READ description WRITE setDescription)
    Q_PROPERTY(QString timestamp READ timestamp WRITE setTimestamp)
    Q_PROPERTY(QPixmap icon READ icon WRITE setIcon)
    Q_PROPERTY(bool busyIndicatorVisible READ busyIndicatorVisible WRITE setBusyIndicatorVisible)
    Q_PROPERTY(bool progressBarVisible READ progressBarVisible WRITE setProgressBarVisible)

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

    // Does not take ownership.
    QnImageProvider* preview() const;
    void setPreview(QnImageProvider* value);

    QRectF previewCropRect() const;
    void setPreviewCropRect(const QRectF& relativeRect);

    CommandActionPtr action() const;
    void setAction(const CommandActionPtr& value);

    bool hasAutoClose() const;
    int autoCloseTimeMs() const;
    int autoCloseRemainingMs() const;
    void setAutoCloseTimeMs(int value);

    bool busyIndicatorVisible() const;
    void setBusyIndicatorVisible(bool value);

    bool progressBarVisible() const;
    void setProgressBarVisible(bool value);

    qreal progressValue() const;
    void setProgressValue(qreal value);

    QString progressTitle() const;
    void setProgressTitle(const QString& value);

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
    void updateBackgroundRole(bool hovered);

private:
    QScopedPointer<Ui::EventTile> ui;
    QPushButton* const m_closeButton = nullptr;
    bool m_closeable = false;
    CommandActionPtr m_action; //< Button action.
    QnElidedLabel* const m_progressLabel = nullptr;
    QTimer* m_autoCloseTimer = nullptr;
    qreal m_progressValue = 0.0;
};

} // namespace
} // namespace client
} // namespace nx
