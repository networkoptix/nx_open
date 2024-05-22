// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QLabel>
#include <chrono>

#include <QtCore/QCache>
#include <QtCore/QTimer>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <ui/widgets/common/elided_label.h>

#include "../event_tile.h"

namespace nx::vms::client::desktop {

class EventTile::Private: public QObject
{
public:
    using TextFormatter = std::function<QString()>;
    struct LabelDescriptor
    {
        QVariant fullContent;
        QCache<int, QString> textByWidth; //< key - width of label, value formatted text
        int currentWidth;
        TextFormatter formatter;
    };

    enum class State
    {
        hoverOn,
        hoverOff,
        pressed,
    };

    EventTile* const q;
    CloseButton* const closeButton;
    WidgetAnchor* const closeButtonAnchor;
    bool closeable = false;
    std::unique_ptr<QAction> action; //< Button action.
    std::unique_ptr<QAction> additionalAction;
    QnElidedLabel* const progressLabel;
    const QScopedPointer<QTimer> loadPreviewTimer;
    bool isPreviewLoadNeeded = true;
    bool forceNextPreviewUpdate = false;
    std::optional<qreal> progressValue = 0.0;
    bool isRead = false;
    bool footerEnabled = true;
    Style style = Style::standard;
    bool highlighted = false;
    QPalette defaultTitlePalette;
    Qt::MouseButton clickButton = Qt::NoButton;
    Qt::KeyboardModifiers clickModifiers;
    QPoint clickPoint;
    LabelDescriptor titleLabelDescriptor;
    LabelDescriptor descriptionLabelDescriptor;
    LabelDescriptor resourceLabelDescriptor;
    QString iconPath;
    bool previewEnabled = false;

    Private(EventTile* q);

    void initLabelDescriptor(const QVariant& value, LabelDescriptor& descriptor);
    void setDescription(const QString& value);
    void clearLabel(QLabel* label, LabelDescriptor& descriptor);
    void setTitle(const QString& value);
    void updateLabelForCurrentWidth(QLabel* label, LabelDescriptor& descriptor);
    void handleStateChanged(State state);
    void updatePalette();
    void updateIcon();
    qreal getWidthOfText(const QString& text, const QLabel* label) const;
    QString getElidedResourceText(const QStringList& list);
    void setResourceList(const QStringList& list);
    bool isPreviewNeeded() const;
    bool isPreviewUpdateRequired() const;
    void requestPreview();
    void updatePreview(std::chrono::milliseconds delay);
    void showDebugPreviewTimestamp();
    void updatePreviewsVisibility();
    void setWidgetHolder(QWidget* widget, QWidget* newHolder);
    QString getSystemName(const QString& systemId);

    // For cloud notifications the system name must be displayed. The function returns the name of
    // a system different from the current one in the form required for display.
    QString getFormattedSystemNameIfNeeded(const QString& systemId);

    QString getElidedStringByRowsNumberAndWidth(
        const QFont& font, const QString& text, int textWidth, int rowLimit);
};

} // namespace nx::vms::client::desktop
