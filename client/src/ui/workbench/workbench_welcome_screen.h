
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>

class QQuickView;
class QnLoginDialog;

class QnWorkbenchWelcomeScreen : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool isVisible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool isEnabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged);

    typedef Connective<QObject> base_type;

public:
    QnWorkbenchWelcomeScreen(QObject *parent);

    virtual ~QnWorkbenchWelcomeScreen();
    
    QWidget *widget();

public: // Properties
    bool isVisible() const;

    void setVisible(bool isVisible);

    bool isEnabled() const;

    void setEnabled(bool isEnabled);

public slots:
    void connectToAnotherSystem();

    void tryHideScreen();

signals:
    void visibleChanged();
    
    void enabledChanged();

private:
    void showScreen();

    void enableScreen();


private:
    typedef QPointer<QWidget> WidgetPtr;
    typedef QPointer<QnLoginDialog> LoginDialogPtr;

    const WidgetPtr m_widget;
    LoginDialogPtr m_loginDialog;

    bool m_visible;
    bool m_enabled;
};