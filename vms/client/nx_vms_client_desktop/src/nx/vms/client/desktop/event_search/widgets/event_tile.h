#pragma once

#include <chrono>

#include <QtWidgets/QAction>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/customization/customized.h>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>

class QModelIndex;
class QPushButton;
class QnElidedLabel;

namespace Ui { class EventTile; }

namespace nx::vms::client::desktop {

class ImageProvider;
class CloseButton;

class EventTile: public Customized<QWidget>
{
    Q_OBJECT
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

    QString footerText() const;
    void setFooterText(const QString& value);

    QString timestamp() const;
    void setTimestamp(const QString& value);

    QPixmap icon() const;
    void setIcon(const QPixmap& value);

    // Does not take ownership.
    ImageProvider* preview() const;
    void setPreview(ImageProvider* value);

    QRectF previewCropRect() const;
    void setPreviewCropRect(const QRectF& relativeRect);

    CommandActionPtr action() const;
    void setAction(const CommandActionPtr& value);

    bool busyIndicatorVisible() const;
    void setBusyIndicatorVisible(bool value);

    bool progressBarVisible() const;
    void setProgressBarVisible(bool value);

    qreal progressValue() const;
    void setProgressValue(qreal value);

    QString progressTitle() const;
    void setProgressTitle(const QString& value);

    bool previewEnabled() const;
    void setPreviewEnabled(bool value);

    bool footerEnabled() const;
    void setFooterEnabled(bool value);

    bool headerEnabled() const;
    void setHeaderEnabled(bool value);

    void setResourceList(const QnResourceList& list); //< Doesn't store it, only generates text.
    void setResourceList(const QStringList& list);

    enum class Mode
    {
        standard,
        wide
    };

    Mode mode() const;
    void setMode(Mode value);

    enum class Style
    {
        standard,
        informer
    };

    Style visualStyle() const;
    void setVisualStyle(Style value);

    bool highlighted() const;
    void setHighlighted(bool value);

    void clear();

signals:
    void clicked(Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void doubleClicked();
    void dragStarted(const QPoint& pos, const QSize& size);

    void closeRequested();

    // Links can be passed and displayed in description field.
    void linkActivated(const QString& link);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual QSize minimumSizeHint() const override;
    virtual bool event(QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
    nx::utils::ImplPtr<Ui::EventTile> ui;
};

} // namespace nx::vms::client::desktop
