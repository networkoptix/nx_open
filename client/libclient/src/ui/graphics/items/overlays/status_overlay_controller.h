#pragma once

#include <core/resource/resource_fwd.h>
#include <client/client_globals.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>

#include <utils/common/connective.h>

class QnStatusOverlayWidget;

class QnStatusOverlayController: public Connective<QObject>
{
    Q_OBJECT

    typedef QPointer<QnStatusOverlayWidget> StatusOverlayWidgetPtr;
    using base_type = Connective<QObject>;
public:
    enum class Button
    {
        kNoButton,
        kDiagnostics,
        kIoEnable,
        kMoreLicenses,
        kSettings
    };

    QnStatusOverlayController(const QnResourcePtr& resource,
        const StatusOverlayWidgetPtr& widget, QObject* parent = nullptr);

    virtual ~QnStatusOverlayController() = default;

    Qn::ResourceStatusOverlay statusOverlay() const;
    void setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay, bool animated);

    Button currentButton() const;
    void setCurrentButton(Button button);

    QString caption() const;

    bool isErrorOverlay() const;

    QnStatusOverlayWidget::Controls visibleItems() const;

public:
    QString currentButtonText() const;

    static QString captionText(Qn::ResourceStatusOverlay overlay);
    static QString descriptionText(Qn::ResourceStatusOverlay overlay);
    static QString statusIcon(Qn::ResourceStatusOverlay overlay);

signals:
    void statusOverlayChanged(bool animated);

    void currentButtonChanged();

    void isErrorOverlayChanged();

    void buttonClicked();

    void visibleItemsChanged();

private:
    void updateVisibleItems();

    void updateWidgetItems();

    void onStatusOverlayChanged(bool animated);

    QnStatusOverlayWidget::Controls errorVisibleItems();
    QnStatusOverlayWidget::Controls normalVisibleItems();

    void updateErrorState();

    typedef QHash<int, QString> IntStringHash;
    static IntStringHash getButtonCaptions(const QnResourcePtr& resource);

private:
    const StatusOverlayWidgetPtr m_widget;
    const IntStringHash m_buttonTexts;

    QnStatusOverlayWidget::Controls m_visibleItems;
    Qn::ResourceStatusOverlay m_statusOverlay;
    Button m_currentButton;
    bool m_isErrorOverlay;
};