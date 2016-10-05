#pragma once

#include <core/resource/resource_fwd.h>
#include <client/client_globals.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>

#include <utils/common/connective.h>

class QnStatusOverlayWidget;

class QnStatusOverlayController: public Connective<QObject>
{
    Q_OBJECT

    Q_PROPERTY(QnStatusOverlayWidget::Controls visibleItems READ visibleItems
        NOTIFY visibleItemsChanged);
    Q_PROPERTY(Qn::ResourceStatusOverlay statusOverlay READ statusOverlay
        WRITE setStatusOverlay NOTIFY statusOverlayChanged)
    Q_PROPERTY(Button currentButton READ currentButton WRITE setCurrentButton
        NOTIFY currentButtonChanged)
    Q_PROPERTY(bool isErrorOverlay READ isErrorOverlay NOTIFY isErrorOverlayChanged)

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
    void setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay);

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
    void statusOverlayChanged();

    void currentButtonChanged();

    void isErrorOverlayChanged();

    void buttonClicked();

    void visibleItemsChanged();

private:
    void updateVisibleItems();

    void updateWidgetItems();

    void onStatusOverlayChanged();

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