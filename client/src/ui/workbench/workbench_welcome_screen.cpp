
#include "workbench_welcome_screen.h"

#include <QtQuick/QQuickView>
#include <QtQml/QQmlContext>

#include <common/common_module.h>

#include <ui/actions/actions.h>
#include <ui/models/systems_model.h>
#include <ui/workbench/workbench_context.h>

#include <ui/dialogs/login_dialog.h>
#include <ui/dialogs/non_modal_dialog_constructor.h>

namespace
{
    typedef QPointer<QnWorkbenchWelcomeScreen> GuardType;

    QQuickView *createMainView(QObject *context)
    {
        static const auto kWelcomeScreenSource = lit("qrc:/src/qml/WelcomeScreen.qml");
        static const auto kContextVariableName = lit("context");

        qmlRegisterType<QnSystemsModel>("Networkoptix", 1, 0, "QnSystemsModel");

        const auto quickView = new QQuickView();
        quickView->rootContext()->setContextProperty(
            kContextVariableName, context);
        quickView->setSource(kWelcomeScreenSource);

        return quickView;
    }
}

QnWorkbenchWelcomeScreen::QnWorkbenchWelcomeScreen(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_widget(QWidget::createWindowContainer(createMainView(this)))
    , m_loginDialog()
    , m_visible(false)
    , m_enabled(false)
{
    const auto idChangedHandler = [this](const QnUuid &id)
    {
        // We could be just reconnecting if remoteGuid is null.
        // So, just hide screen when it is not
        if (!qnCommon->remoteGUID().isNull())
            setVisible(false);
    };

    connect(qnCommon, &QnCommonModule::remoteIdChanged
        , this, idChangedHandler);
    connect(action(QnActions::DisconnectAction), &QAction::triggered
        , this, &QnWorkbenchWelcomeScreen::showScreen);

    connect(this, &QnWorkbenchWelcomeScreen::visibleChanged, this, [this]()
    {
        context()->action(QnActions::EscapeHotkeyAction)->setEnabled(!m_visible);

        if (isVisible())
            enableScreen();
    });

    setVisible(true);
}

QnWorkbenchWelcomeScreen::~QnWorkbenchWelcomeScreen()
{}

QWidget *QnWorkbenchWelcomeScreen::widget()
{
    return m_widget.data();
}

bool QnWorkbenchWelcomeScreen::isVisible() const
{
    return m_visible;
}

void QnWorkbenchWelcomeScreen::setVisible(bool isVisible)
{
    if (m_visible == isVisible)
        return;

    m_visible = isVisible;

    emit visibleChanged();
}

bool QnWorkbenchWelcomeScreen::isEnabled() const
{
    return m_enabled;
}

void QnWorkbenchWelcomeScreen::setEnabled(bool isEnabled)
{
    if (m_enabled == isEnabled)
        return;

    m_enabled = isEnabled;
    emit enabledChanged();
}

void QnWorkbenchWelcomeScreen::connectToAnotherSystem()
{
    bool wasEmpty = (m_loginDialog == nullptr);
    QnNonModalDialogConstructor<QnLoginDialog> dialogConstructor(
        m_loginDialog, m_widget.data());

    if (wasEmpty)
    {
        connect(m_loginDialog, &QDialog::finished
            , this, &QnWorkbenchWelcomeScreen::enableScreen);
    }

    dialogConstructor.resetGeometry();
    setEnabled(false);
}

void QnWorkbenchWelcomeScreen::tryHideScreen()
{
    if (!qnCommon->remoteGUID().isNull())
        setVisible(false);
}

void QnWorkbenchWelcomeScreen::enableScreen()
{
    setEnabled(true);
}

void QnWorkbenchWelcomeScreen::showScreen()
{
    setVisible(true);
}
