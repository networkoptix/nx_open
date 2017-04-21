#include "workbench_debug_handler.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>

#include <QtWebKitWidgets/QWebView>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>

#include <ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/widgets/palette_widget.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/widgets/common/web_page.h>
#include <ui/widgets/views/resource_list_view.h>

#include <nx/client/desktop/ui/dialogs/debug/animations_control_dialog.h>
#include <nx/client/desktop/ui/dialogs/debug/applauncher_control_dialog.h>
#include <finders/test_systems_finder.h>
#include <finders/system_tiles_test_case.h>
#include <finders/systems_finder.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_welcome_screen.h>

#ifdef _DEBUG
#define DEBUG_ACTIONS
#endif

namespace {


class QnWebViewDialog: public QDialog
{
    using base_type = QDialog;
public:
    QnWebViewDialog(QWidget* parent = nullptr):
        base_type(parent, Qt::Window),
        m_page(new QnWebPage(this)),
        m_webView(new QWebView(this)),
        m_urlLineEdit(new QLineEdit(this))
    {
        m_webView->setPage(m_page);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(QMargins());
        layout->addWidget(m_urlLineEdit);
        layout->addWidget(m_webView);
        connect(m_urlLineEdit, &QLineEdit::returnPressed, this, [this]()
            {
                m_webView->load(m_urlLineEdit->text());
            });
    }

    QString url() const { return m_urlLineEdit->text(); }
    void setUrl(const QString& value)
    {
        m_urlLineEdit->setText(value);
        m_webView->load(QUrl::fromUserInput(value));
    }

private:
    QnWebPage* m_page;
    QWebView* m_webView;
    QLineEdit* m_urlLineEdit;
};

// -------------------------------------------------------------------------- //
// QnDebugControlDialog
// -------------------------------------------------------------------------- //
class QnDebugControlDialog: public QnDialog, public QnWorkbenchContextAware
{
    typedef QnDialog base_type;

public:
    QnDebugControlDialog(QWidget *parent = NULL):
        base_type(parent),
        QnWorkbenchContextAware(parent)
    {
        using namespace nx::client::desktop::ui;

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(newActionButton(QnActions::DebugDecrementCounterAction));
        layout->addWidget(newActionButton(QnActions::DebugIncrementCounterAction));

        auto addButton = [this, parent, layout]
            (const QString& name, std::function<void(void)> handler)
            {
                auto button = new QPushButton(name, parent);
                connect(button, &QPushButton::clicked, handler);
                layout->addWidget(button);
            };

        addButton(lit("Applaucher control"), [this] { (new QnApplauncherControlDialog(this))->show();});

        addButton(lit("Animations control"), [this] { (new AnimationsControlDialog(this))->show(); });

        addButton(lit("Web View"), [this]
            {
                auto dialog(new QnWebViewDialog(this));
                dialog->setUrl(lit("http://localhost:7001"));
                dialog->show();
            });

        addButton(lit("Palette"), [this]
            {
                QnPaletteWidget *w = new QnPaletteWidget(this);
                w->setPalette(qApp->palette());
                auto messageBox = new QnMessageBox(mainWindow());
                messageBox->setWindowFlags(Qt::Window);
                messageBox->addCustomWidget(w);
                messageBox->show();
            });

        addButton(lit("Resource Pool"), [this]
            {
                auto messageBox = new QnMessageBox(mainWindow());
                messageBox->setWindowFlags(Qt::Window);
                messageBox->addCustomWidget(new QnResourceListView(resourcePool()->getResources(), messageBox));
                messageBox->show();
            });

        addButton(lit("Tiles tests"),
            [this]()
            {
                runTilesTest();
                close();
            });

    }

private:
    QToolButton *newActionButton(QnActions::IDType actionId, QWidget *parent = NULL)
    {
        QToolButton *button = new QToolButton(parent);
        button->setDefaultAction(menu()->action(actionId));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return button;
    }

    static QnSystemTilesTestCase *m_tilesTests;

    void runTilesTest()
    {
        if (!m_tilesTests)
        {
            static constexpr auto kSomeFarPriority = 1000;
            const auto testSystemsFinder = new QnTestSystemsFinder(qnSystemsFinder);
            qnSystemsFinder->addSystemsFinder(testSystemsFinder, kSomeFarPriority);

            m_tilesTests = new QnSystemTilesTestCase(testSystemsFinder, this);

            const auto welcomeScreen = context()->instance<QnWorkbenchWelcomeScreen>();

            connect(m_tilesTests, &QnSystemTilesTestCase::openTile,
                welcomeScreen, &QnWorkbenchWelcomeScreen::openTile);
            connect(m_tilesTests, &QnSystemTilesTestCase::switchPage,
                welcomeScreen, &QnWorkbenchWelcomeScreen::switchPage);
            connect(m_tilesTests, &QnSystemTilesTestCase::collapseExpandedTile, this,
                [welcomeScreen]() { emit welcomeScreen->openTile(QString());});
            connect(m_tilesTests, &QnSystemTilesTestCase::restoreApp, this,
                [this]()
                {
                    const auto maximizeAction = action(QnActions::FullscreenAction);
                    if (maximizeAction->isChecked())
                        maximizeAction->toggle();
                });
            connect(m_tilesTests, &QnSystemTilesTestCase::makeFullscreen, this,
                [this]()
                {
                    const auto maximizeAction = action(QnActions::FullscreenAction);
                    if (!maximizeAction->isChecked())
                        maximizeAction->toggle();
                });

            connect(m_tilesTests, &QnSystemTilesTestCase::messageChanged,
                welcomeScreen, &QnWorkbenchWelcomeScreen::setMessage);
        }

        m_tilesTests->runTestSequence(QnTileTest::First);
    }
};

QnSystemTilesTestCase *QnDebugControlDialog::m_tilesTests = nullptr;


} // namespace

// -------------------------------------------------------------------------- //
// QnWorkbenchDebugHandler
// -------------------------------------------------------------------------- //
QnWorkbenchDebugHandler::QnWorkbenchDebugHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
#ifdef DEBUG_ACTIONS
    //TODO: #GDM #High remove before release
    qDebug() << "------------- Debug actions ARE ACTIVE -------------";
    connect(action(QnActions::DebugControlPanelAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered);
    connect(action(QnActions::DebugIncrementCounterAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered);
    connect(action(QnActions::DebugDecrementCounterAction), &QAction::triggered, this,
        &QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered);
#endif
}

void QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered()
{
    QnDebugControlDialog* dialog(new QnDebugControlDialog(mainWindow()));
    dialog->show();
}

void QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() + 1);
    qDebug() << qnRuntime->debugCounter();
}

void QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered()
{
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() - 1);
    qDebug() << qnRuntime->debugCounter();
}
