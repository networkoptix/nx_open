// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>

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
    QnStatusOverlayWidget::ErrorStyle errorStyle() const;
    QnStatusOverlayWidget::Controls visibleItems() const;

public:
    QString currentButtonText() const;

    static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions statusIconColors(Qn::ResourceStatusOverlay overlay);
    static QPixmap statusIcon(Qn::ResourceStatusOverlay overlay);

signals:
    void statusOverlayChanged(bool animated);
    void currentButtonChanged();
    void customButtonTextChanged();
    void isErrorOverlayChanged();
    void isErrorStyleChanged();
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
    void updateErrorStyle();

    typedef QHash<int, QString> IntStringHash;
    static IntStringHash getButtonCaptions(const QnResourcePtr& resource);

private:
    QnResourcePtr m_resource;
    const StatusOverlayWidgetPtr m_widget;
    const IntStringHash m_buttonTexts;

    QnStatusOverlayWidget::Controls m_visibleItems;
    Qn::ResourceStatusOverlay m_statusOverlay;
    Qn::ResourceOverlayButton m_currentButton;
    QString m_customButtonText;

    bool m_isErrorOverlay;
    QnStatusOverlayWidget::ErrorStyle m_errorStyle;
};
