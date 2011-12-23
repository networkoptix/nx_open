#ifndef FANCYMAINWINDOW_H
#define FANCYMAINWINDOW_H

#include <QtCore/QHash>
#include <QtCore/QPointer>

#include <QtGui/QFrame>

class QAbstractButton;
class QLabel;
class QPushButton;
class QToolButton;

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = 0);
    virtual ~TitleBar();

protected:
    enum Change {
        WindowStateHasChanged = 0x01,
        TitleHasChanged = 0x02,
        SystemMenuHasChanged = 0x04,
        StyleHasChanged = 0x08,
        LanguageHasChanged = 0x10,

        WindowChange = WindowStateHasChanged |
                       TitleHasChanged | SystemMenuHasChanged |
                       StyleHasChanged | LanguageHasChanged,
        WindowFlagsHasChanged = WindowChange
    };
    Q_DECLARE_FLAGS(Changes, Change)

    void updateControls(Changes change);

protected:
    bool eventFilter(QObject *watched, QEvent *event);
    void changeEvent(QEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);

private Q_SLOTS:
    void minimizeWindow();
    void maximizeRestoreWindow();
    void closeWindow();
    void systemMenuButtonClicked();

private:
    Q_DISABLE_COPY(TitleBar)

    Qt::WindowFlags m_oldWindowFlags;

    QLabel *m_titleLabel;
    QPushButton *systemMenuButton;
    QToolButton *contextHelpButton;
    QToolButton *fullScreenButton;
    QToolButton *minimizeButton;
    QToolButton *maximizeButton;
    QToolButton *closeButton;
    QPoint m_clickPos;
};


class FancyMainWindow : public QFrame
{
    Q_OBJECT

public:
    explicit FancyMainWindow(QWidget *parent = 0, Qt::WFlags flags = 0);

    TitleBar *titleBar() const;

    // Allows you to access the content area of the frame
    // where widgets and layouts can be added
    QWidget *centralWidget() const;
    void setCentralWidget(QWidget *widget);

    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);

private:
    QPointer<TitleBar> m_titleBar;
    QPointer<QWidget> m_centralWidget;
    QLayout *m_centralLayout;
    QPoint m_old_pos;
    bool m_mouse_down;
    bool left, right, bottom;
};

#endif // FANCYMAINWINDOW_H
