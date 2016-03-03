
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>

class QQuickView;

class QnWorkbenchWelcomeScreen : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool isVisible READ isVisible NOTIFY visibleChanged)

    typedef Connective<QObject> base_type;

public:
    QnWorkbenchWelcomeScreen(QObject *parent);

    virtual ~QnWorkbenchWelcomeScreen();
    
    bool isVisible() const;

    QWidget *widget();

public slots:
    QObject *createSystemsModel();

signals:
    void visibleChanged();

private:
    typedef QScopedPointer<QQuickView> QuickViewPtr;
    typedef QPointer<QWidget> WidgetPtr;

    const QuickViewPtr m_quickView;
    const WidgetPtr m_widget;
};