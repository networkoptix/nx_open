
#include "workbench_welcome_screen.h"

#include <QtQuick/QQuickView>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine> // todo: remove

#include <common/common_module.h>
#include <ui/models/systems_model.h>
#include <ui/workbench/workbench_context.h>

QnWorkbenchWelcomeScreen::QnWorkbenchWelcomeScreen(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_quickView(new QQuickView())
    , m_widget(QWidget::createWindowContainer(m_quickView.data()))
{
    static const auto kWelcomeScreenSource = lit("qrc:/src/qml/WelcomeScreen.qml");
    static const auto kContextVariableName = lit("context");

    m_quickView->rootContext()->setContextProperty(
        kContextVariableName, this);
    m_quickView->setSource(kWelcomeScreenSource);

    qDebug() << "----------------" << m_quickView->engine()->importPathList();
    connect(qnCommon, &QnCommonModule::remoteIdChanged
        , this, &QnWorkbenchWelcomeScreen::visibleChanged);
}

QnWorkbenchWelcomeScreen::~QnWorkbenchWelcomeScreen()
{}

bool QnWorkbenchWelcomeScreen::isVisible() const
{
    return qnCommon->remoteGUID().isNull();
}

QWidget *QnWorkbenchWelcomeScreen::widget()
{
    return m_widget.data();
}

QObject *QnWorkbenchWelcomeScreen::createSystemsModel()
{
    return (new QnSystemsModel(this));
}

