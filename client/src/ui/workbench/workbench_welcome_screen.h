
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/style/generic_palette.h>
class QnCloudStatusWatcher;

class QnWorkbenchWelcomeScreen : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

    Q_PROPERTY(bool isVisible READ isVisible WRITE setVisible NOTIFY visibleChanged)

    Q_PROPERTY(QString cloudUserName READ cloudUserName NOTIFY cloudUserNameChanged);
    Q_PROPERTY(bool isLoggedInToCloud READ isLoggedInToCloud NOTIFY isLoggedInToCloudChanged)

    Q_PROPERTY(QSize pageSize READ pageSize WRITE setPageSize NOTIFY pageSizeChanged);
    Q_PROPERTY(bool visibleControls READ visibleControls WRITE setVisibleControls NOTIFY visibleControlsChanged)
    Q_PROPERTY(bool connectingNow READ connectingNow WRITE setConnectingNow NOTIFY connectingNowChanged)

public:
    QnWorkbenchWelcomeScreen(QObject *parent);

    virtual ~QnWorkbenchWelcomeScreen();

    QWidget *widget();

public: // Properties
    bool isVisible() const;

    void setVisible(bool isVisible);

    QString cloudUserName() const;

    bool isLoggedInToCloud() const;

    QSize pageSize() const;

    void setPageSize(const QSize &size);

    bool visibleControls() const;

    void setVisibleControls(bool visible);

    bool connectingNow() const;

    void setConnectingNow(bool value);

public slots:
    // TODO: $ynikitenkov add multiple urls one-by-one  handling
    void connectToLocalSystem(const QString &serverUrl
        , const QString &userName
        , const QString &password);

    void connectToCloudSystem(const QString &serverUrl);

    void connectToAnotherSystem();

    void setupFactorySystem(const QString &serverUrl);

    void logoutFromCloud();

    void manageCloudAccount();

    void loginToCloud();

    void createAccount();

    void tryHideScreen();

public slots:
    QColor getPaletteColor(const QString &group
        , int index);

    QColor getDarkerColor(const QColor &color
        , int offset = 1);

    QColor getLighterColor(const QColor &color
        , int offset = 1);

    QColor colorWithAlpha(QColor color
        , qreal alpha);

signals:
    void visibleChanged();

    void cloudUserNameChanged();

    void isLoggedInToCloudChanged();

    void pageSizeChanged();

    void visibleControlsChanged();

    void connectingNowChanged();

private:
    void showScreen();

    void enableScreen();

private: // overrides
    bool eventFilter(QObject *obj
        , QEvent *event) override;

private:
    typedef QPointer<QWidget> WidgetPtr;
    typedef QPointer<QnCloudStatusWatcher> CloudStatusWatcherPtr;

    bool m_visibleControls;
    bool m_visible;
    bool m_connectingNow;
    const QnGenericPalette m_palette;
    const WidgetPtr m_widget;
    QSize m_pageSize;
};