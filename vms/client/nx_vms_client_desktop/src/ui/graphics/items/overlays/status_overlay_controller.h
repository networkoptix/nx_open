// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>

class QnStatusOverlayWidget;

class QnStatusOverlayController: public QObject
{
    Q_OBJECT

    typedef QPointer<QnStatusOverlayWidget> StatusOverlayWidgetPtr;
    using base_type = QObject;
public:
    QnStatusOverlayController(const QnResourcePtr& resource,
        const StatusOverlayWidgetPtr& widget, QObject* parent = nullptr);

    virtual ~QnStatusOverlayController() = default;

    Qn::ResourceStatusOverlay statusOverlay() const;
    void setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay, bool animated);

    Qn::ResourceOverlayButton currentButton() const;
    void setCurrentButton(Qn::ResourceOverlayButton button);

    QString customButtonText() const;
    void setCustomButtonText(const QString& text);

    bool isErrorOverlay() const;

    QnStatusOverlayWidget::Controls visibleItems() const;

public:
    QString currentButtonText() const;

    static QString captionText(Qn::ResourceStatusOverlay overlay);
    static QString descriptionText(Qn::ResourceStatusOverlay overlay);
    static QString suggestionText(Qn::ResourceStatusOverlay overlay, bool buttonPresent);
    static QString statusIconPath(Qn::ResourceStatusOverlay overlay);
    static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions statusIconColors(Qn::ResourceStatusOverlay overlay);
    static QPixmap statusIcon(Qn::ResourceStatusOverlay overlay);

signals:
    void statusOverlayChanged(bool animated);

    void currentButtonChanged();

    void customButtonTextChanged();

    void isErrorOverlayChanged();

    void buttonClicked(Qn::ResourceOverlayButton button);

    void customButtonClicked();

    void visibleItemsChanged();

private:
    void updateVisibleItems();

    void updateWidgetItems();

    void onStatusOverlayChanged(bool animated);

    QnStatusOverlayWidget::Controls errorVisibleItems() const;
    QnStatusOverlayWidget::Controls normalVisibleItems() const;

    void updateErrorState();

    typedef QHash<int, QString> IntStringHash;
    static IntStringHash getButtonCaptions(const QnResourcePtr& resource);

private:
    const StatusOverlayWidgetPtr m_widget;
    const IntStringHash m_buttonTexts;

    QnStatusOverlayWidget::Controls m_visibleItems;
    Qn::ResourceStatusOverlay m_statusOverlay;
    Qn::ResourceOverlayButton m_currentButton;
    QString m_customButtonText;

    bool m_isErrorOverlay;
};
