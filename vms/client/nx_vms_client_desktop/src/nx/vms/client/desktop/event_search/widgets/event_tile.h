// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QAction>
#include <QtWidgets/QWidget>

#include <analytics/common/object_metadata.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>

namespace Ui { class EventTile; }

namespace nx::vms::client::desktop {

class ImageProvider;
class CloseButton;

class EventTile: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit EventTile(QWidget* parent = nullptr);

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

    core::analytics::AttributeList attributeList() const;
    void setAttributeList(const core::analytics::AttributeList& value);

    QString timestamp() const;
    void setTimestamp(const QString& value);

    QPixmap icon() const;
    void setIcon(const QPixmap& value);

    // Does not take ownership.
    ImageProvider* preview() const;
    void setPreview(ImageProvider* value, bool forceUpdate);

    void setPlaceholder(const QString& text);
    void setForcePreviewLoader(bool force);

    QRectF previewHighlightRect() const;
    void setPreviewHighlightRect(const QRectF& relativeRect);

    CommandActionPtr action() const;
    void setAction(const CommandActionPtr& value);

    CommandActionPtr additionalAction() const;
    void setAdditionalAction(const CommandActionPtr& value);

    bool busyIndicatorVisible() const;
    void setBusyIndicatorVisible(bool value);

    bool progressBarVisible() const;
    void setProgressBarVisible(bool value);

    std::optional<qreal> progressValue() const;
    void setProgressValue(qreal value);
    void setIndefiniteProgress();

    QString progressTitle() const;
    void setProgressTitle(const QString& value);

    QString progressFormat() const;
    void setProgressFormat(const QString& value);

    bool previewEnabled() const;
    void setPreviewEnabled(bool value);

    bool automaticPreviewLoad() const;
    void setAutomaticPreviewLoad(bool value);
    bool isPreviewLoadNeeded() const;

    bool footerEnabled() const;
    void setFooterEnabled(bool value);

    bool headerEnabled() const;
    void setHeaderEnabled(bool value);

    // Resources are not stored, function only generates text.
    void setResourceList(const QnResourceList& list, const QString& cloudSystemId);
    void setResourceList(const QStringList& list, const QString& cloudSystemId);

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

    void needsPreviewLoad();

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual QSize minimumSizeHint() const override;
    virtual bool event(QEvent* event) override;
    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
    nx::utils::ImplPtr<Ui::EventTile> ui;
};

} // namespace nx::vms::client::desktop
