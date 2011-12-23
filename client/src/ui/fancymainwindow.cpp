#include "fancymainwindow.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QStyle>
#include <QtGui/QToolButton>

static Qt::WindowFlags adjustFlags(Qt::WindowFlags flags, QWidget *window)
{
    bool customize = (flags & (Qt::CustomizeWindowHint
                               | Qt::FramelessWindowHint
                               | Qt::WindowTitleHint
                               | Qt::WindowSystemMenuHint
                               | Qt::WindowMinimizeButtonHint
                               | Qt::WindowMaximizeButtonHint
                               | Qt::WindowCloseButtonHint
                               | Qt::WindowContextHelpButtonHint
                               | Qt::WindowShadeButtonHint));
    uint type = (flags & Qt::WindowType_Mask);
    if ((type == Qt::Widget || type == Qt::SubWindow) && window && !window->parent()) {
        type = Qt::Window;
        flags |= Qt::Window;
    }

    if (customize)
        ; // don't modify window flags if the user explicitly set them.
    else if (type == Qt::Dialog || type == Qt::Sheet || type == Qt::Tool)
        flags |= Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint;
    else
        flags |= Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint;

    return flags;
}


TitleBar::TitleBar(QWidget *parent)
    : QWidget(0, Qt::FramelessWindowHint),
      m_oldWindowFlags(0)
{
    setMouseTracking(true);

    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Window);
    setForegroundRole(QPalette::WindowText);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setContentsMargins(0, 0, 0, 0);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_titleLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::Label));
    m_titleLabel->setMargin(8);
    m_titleLabel->setWordWrap(false);
    {
        QFont titleFont = font();
        titleFont.setBold(true);
        m_titleLabel->setFont(titleFont);
    }

    systemMenuButton = new QPushButton(this);
    systemMenuButton->setFocusPolicy(Qt::NoFocus);
    systemMenuButton->setFlat(true);

    contextHelpButton = new QToolButton(this);
    contextHelpButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    contextHelpButton->setFocusPolicy(Qt::NoFocus);

    fullScreenButton = new QToolButton(this);
    fullScreenButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    fullScreenButton->setFocusPolicy(Qt::NoFocus);

    minimizeButton = new QToolButton(this);
    minimizeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    minimizeButton->setFocusPolicy(Qt::NoFocus);

    maximizeButton = new QToolButton(this);
    maximizeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    maximizeButton->setFocusPolicy(Qt::NoFocus);

    closeButton = new QToolButton(this);
    closeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    closeButton->setFocusPolicy(Qt::NoFocus);

    connect(systemMenuButton, SIGNAL(clicked()), this, SLOT(systemMenuButtonClicked()));
    connect(minimizeButton, SIGNAL(clicked()), this, SLOT(minimizeWindow()));
    connect(maximizeButton, SIGNAL(clicked()), this, SLOT(maximizeRestoreWindow()));
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeWindow()));


    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    layout->addSpacing(4);
    layout->addWidget(systemMenuButton, 0, Qt::AlignVCenter);
    layout->addWidget(m_titleLabel, 100, Qt::AlignVCenter);
    layout->addWidget(contextHelpButton, 0, Qt::AlignVCenter);
    layout->addSpacing(2);
    layout->addWidget(fullScreenButton, 0, Qt::AlignVCenter);
    layout->addSpacing(1);
    layout->addWidget(minimizeButton, 0, Qt::AlignVCenter);
    layout->addWidget(maximizeButton, 0, Qt::AlignVCenter);
    layout->addSpacing(2);
    layout->addWidget(closeButton, 0, Qt::AlignVCenter);
    setLayout(layout);

    setParent(parent); // posts ParentChange event
}

TitleBar::~TitleBar()
{
}

void TitleBar::updateControls(TitleBar::Changes change)
{
    QWidget *window = this->window();
    Qt::WindowStates windowState = window->windowState();
    Qt::WindowFlags windowFlags = adjustFlags(window->windowFlags(), window);
    if ((windowFlags & Qt::CustomizeWindowHint) == 0) {
        windowFlags |= Qt::WindowSystemMenuHint | Qt::WindowTitleHint
                       | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint;
    }

    if (m_oldWindowFlags != windowFlags) {
        m_oldWindowFlags = windowFlags;
        change = WindowFlagsHasChanged;
    }

    if (change == WindowChange) {
        systemMenuButton->setVisible(windowFlags & Qt::WindowSystemMenuHint);
        contextHelpButton->setVisible(windowFlags & Qt::WindowContextHelpButtonHint);
        fullScreenButton->setVisible(windowFlags & Qt::WindowShadeButtonHint);
        m_titleLabel->setVisible(windowFlags & Qt::WindowTitleHint);
        minimizeButton->setVisible(windowFlags & Qt::WindowMinimizeButtonHint);
        maximizeButton->setVisible(windowFlags & Qt::WindowMaximizeButtonHint);
        closeButton->setVisible(windowFlags & Qt::WindowCloseButtonHint);

        maximizeButton->setDisabled((windowFlags & Qt::MSWindowsFixedSizeDialogHint)
                                    || (window->minimumSize() == window->maximumSize() && !window->minimumSize().isEmpty())); // ###
    }

    if (change & TitleHasChanged) {
        if (windowFlags & Qt::WindowTitleHint)
            m_titleLabel->setText(window->windowTitle());
    }

    if (change & SystemMenuHasChanged) {
        if (windowFlags & Qt::WindowSystemMenuHint) {
            if (!window->windowIcon().isNull())
                systemMenuButton->setIcon(window->windowIcon());
            else if (!QApplication::windowIcon().isNull())
                systemMenuButton->setIcon(QApplication::windowIcon());
        }
    }

    if (change & (StyleHasChanged | WindowStateHasChanged)) {
        if (windowFlags & Qt::WindowMaximizeButtonHint) {
            QStyle::StandardPixmap icon = (windowState & Qt::WindowMaximized) ? QStyle::SP_TitleBarNormalButton
                                                                              : QStyle::SP_TitleBarMaxButton;
            maximizeButton->setIcon(style()->standardIcon(icon, 0, window));
        }
    }

    if (change & StyleHasChanged) {
        QStyle *style = this->style();

        m_titleLabel->setAlignment(Qt::Alignment(style->styleHint(QStyle::SH_TabBar_Alignment, 0, window)));

        const int metric = qMin(style->pixelMetric(QStyle::PM_ToolBarIconSize, 0, window), 20);
        const QSize iconSize(metric, metric);

        systemMenuButton->setIconSize(iconSize);
        contextHelpButton->setIconSize(iconSize);
        fullScreenButton->setIconSize(iconSize);
        minimizeButton->setIconSize(iconSize);
        maximizeButton->setIconSize(iconSize);
        closeButton->setIconSize(iconSize);

        systemMenuButton->setFixedSize(iconSize);
        contextHelpButton->setFixedSize(iconSize);
        fullScreenButton->setFixedSize(iconSize);
        minimizeButton->setFixedSize(iconSize);
        maximizeButton->setFixedSize(iconSize);
        closeButton->setFixedSize(iconSize);

        const bool buttonsAutoRaise = style->styleHint(QStyle::SH_TitleBar_AutoRaise, 0, window);
        contextHelpButton->setAutoRaise(buttonsAutoRaise);
        fullScreenButton->setAutoRaise(buttonsAutoRaise);
        minimizeButton->setAutoRaise(buttonsAutoRaise);
        maximizeButton->setAutoRaise(buttonsAutoRaise);
        closeButton->setAutoRaise(buttonsAutoRaise);

        contextHelpButton->setIcon(style->standardIcon(QStyle::SP_TitleBarContextHelpButton, 0, window));
        systemMenuButton->setIcon(style->standardIcon(QStyle::SP_TitleBarMenuButton, 0, window));
        minimizeButton->setIcon(style->standardIcon(QStyle::SP_TitleBarMinButton, 0, window));
        closeButton->setIcon(style->standardIcon(QStyle::SP_TitleBarCloseButton, 0, window));
    }

    if (change & (LanguageHasChanged | WindowStateHasChanged)) {
        if (windowFlags & Qt::WindowContextHelpButtonHint)
            contextHelpButton->setToolTip(tr("What This Mode"));
        if (windowFlags & Qt::WindowShadeButtonHint)
            fullScreenButton->setToolTip((windowState & Qt::WindowFullScreen) ? tr("Exit Full Screen") : tr("Full Screen"));
        if (windowFlags & Qt::WindowMinimizeButtonHint)
            minimizeButton->setToolTip(tr("Minimize"));
        if (windowFlags & Qt::WindowMaximizeButtonHint)
            maximizeButton->setToolTip((windowState & Qt::WindowMaximized) ? tr("Restore") : tr("Maximize"));
        if (windowFlags & Qt::WindowCloseButtonHint)
            closeButton->setToolTip(tr("Close"));
    }
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::IconTextChange:
    case QEvent::WindowTitleChange:
        updateControls(TitleHasChanged);
        break;
    case QEvent::WindowStateChange:
        updateControls(WindowStateHasChanged);
        break;
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

void TitleBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    switch (event->type()) {
    case QEvent::MouseTrackingChange:
        if (!hasMouseTracking())
            setMouseTracking(true);
        break;
    case QEvent::ParentAboutToChange:
        window()->removeEventFilter(this);
        break;
    case QEvent::ParentChange:
        if (!QApplication::closingDown()) {
            window()->installEventFilter(this);
            updateControls(WindowChange);
        }
        break;
    case QEvent::FontChange:
        {
            QFont titleFont = font();
            titleFont.setBold(true);
            m_titleLabel->setFont(titleFont);
        }
        break;
    case QEvent::LanguageChange:
        updateControls(LanguageHasChanged);
        break;
    case QEvent::StyleChange:
        updateControls(StyleHasChanged);
        break;
    case QEvent::ApplicationWindowIconChange:
    case QEvent::WindowIconChange:
        updateControls(SystemMenuHasChanged);
        break;
    default:
        break;
    }
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *window = this->window();
        if ((window->windowState() & (Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen)) == 0)
            m_clickPos = mapTo(window, event->pos());
    }

    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        QWidget *window = this->window();
        if ((window->windowState() & (Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen)) == 0)
            window->move(event->globalPos() - m_clickPos);
    }

    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        maximizeRestoreWindow();

    QWidget::mouseDoubleClickEvent(event);
}

void TitleBar::minimizeWindow()
{
    window()->showMinimized();
}

void TitleBar::maximizeRestoreWindow()
{
    QWidget *window = this->window();
    if (window->isMaximized())
        window->showNormal();
    else
        window->showMaximized();
}

void TitleBar::closeWindow()
{
    window()->close();
}

void TitleBar::systemMenuButtonClicked()
{
}


FancyMainWindow::FancyMainWindow(QWidget *parent, Qt::WFlags flags)
    : QFrame(parent, flags | Qt::FramelessWindowHint),
      m_centralWidget(0)
{
    setFrameShape(WinPanel);
    setMouseTracking(true);

    left = right = bottom = false;
    m_mouse_down = false;

    m_titleBar = new TitleBar(this);


    m_centralLayout = new QVBoxLayout;
    m_centralLayout->setContentsMargins(3, 0, 3, 3);
    m_centralLayout->setSpacing(0);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_titleBar);
    layout->addLayout(m_centralLayout, 100);
    setLayout(layout);
}

TitleBar *FancyMainWindow::titleBar() const
{
    return m_titleBar;
}

/*!
    Returns the central widget for the main window. This function
    returns zero if the central widget has not been set.

    \sa setCentralWidget()
*/
QWidget *FancyMainWindow::centralWidget() const
{
    return m_centralWidget;
}

/*!
    Sets the given \a widget to be the main window's central widget.

    Note: QMainWindow takes ownership of the \a widget pointer and
    deletes it at the appropriate time.

    \sa centralWidget()
*/
void FancyMainWindow::setCentralWidget(QWidget *widget)
{
    if (m_centralWidget == widget)
        return;

    if (m_centralWidget) {
        m_centralWidget->hide();
        m_centralLayout->removeWidget(m_centralWidget);
        m_centralWidget->deleteLater();
    }

    m_centralWidget = widget;

    if (m_centralWidget) {
        m_centralLayout->addWidget(m_centralWidget);
        m_centralWidget->show();
    }
}

void FancyMainWindow::mousePressEvent(QMouseEvent *e)
{
    m_old_pos = e->pos();
    m_mouse_down = e->button() == Qt::LeftButton;
}

void FancyMainWindow::mouseMoveEvent(QMouseEvent *e)
{
    int x = e->x();
    int y = e->y();

    if (m_mouse_down) {
        int dx = x - m_old_pos.x();
        int dy = y - m_old_pos.y();

        QRect g = geometry();

        if (left)
            g.setLeft(g.left() + dx);
        if (right)
            g.setRight(g.right() + dx);
        if (bottom)
            g.setBottom(g.bottom() + dy);

        setGeometry(g);

        m_old_pos = QPoint(!left ? e->x() : m_old_pos.x(), e->y());
    } else {
        QRect r = rect();
        left = qAbs(x - r.left()) <= 5;
        right = qAbs(x - r.right()) <= 5;
        bottom = qAbs(y - r.bottom()) <= 5;
        bool hor = left | right;

        if (hor && bottom) {
            if (left)
                setCursor(Qt::SizeBDiagCursor);
            else
                setCursor(Qt::SizeFDiagCursor);
        } else if (hor) {
            setCursor(Qt::SizeHorCursor);
        } else if (bottom) {
            setCursor(Qt::SizeVerCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
}

void FancyMainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_mouse_down = false;
}
