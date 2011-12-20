#ifndef layout_navigator_h_1340
#define layout_navigator_h_1340

#include "graphicsview.h"
#include "mainwnd.h"

class QGraphicsScene;

class LayoutContent;

class CLLayoutNavigator : public QObject
{
    Q_OBJECT

public:
    enum ViewMode{UNDEFINED_ViewMode, NORMAL_ViewMode, LAYOUTEDITOR_ViewMode};

    CLLayoutNavigator(QWidget* mainWnd, LayoutContent* content = 0);
    ~CLLayoutNavigator();

    GraphicsView& getView();
    void destroy();

    void setMode(ViewMode mode);
    ViewMode getMode() const ;

    void goToNewLayoutContent(LayoutContent* newl);

signals:
    void onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*);

protected slots:
    void onDecorationPressed(LayoutContent* layout, QString itemname);
    void onButtonItemPressed(LayoutContent* l, QString itemname);
    void onLayOutStoped(LayoutContent* l);
    void onNewLayoutSelected(LayoutContent* oldl, LayoutContent* newl);
    void onNewLayoutItemSelected(LayoutContent* newl);
    void onIntroScreenEscape();

protected:
    void goToNewLayoutContent();

protected:
    QGraphicsScene *m_scene;
    GraphicsView *m_videoView;

    LayoutContent* mCurrentContent;
    LayoutContent* mNewContent;

    ViewMode m_mode;
};

#endif //layout_navigator_h_1340
