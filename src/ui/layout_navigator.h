#ifndef layout_navigator_h_1340
#define layout_navigator_h_1340

#include <QObject>
#include "graphicsview.h"
#include "mainwnd.h"
class LayoutContent;

class CLLayoutNavigator : public QObject
{
	Q_OBJECT
public:
	CLLayoutNavigator(MainWnd* mainWnd, LayoutContent* content = 0);
	~CLLayoutNavigator();

	GraphicsView& getView();
	void destroy();

	void setMode(MainWnd::ViewMode mode);
	MainWnd::ViewMode getMode() const ;

signals:
	void onItemPressed(QString);

protected slots:
	void onDecorationPressed(LayoutContent* layout, QString itemname);
	void onButtonItemPressed(LayoutContent* l, QString itemname);
	void onLayOutStoped(LayoutContent* l);

	void onNewLayoutSelected(LayoutContent* oldl, LayoutContent* newl);

protected:
	void goToNewLayoutContent();

protected:
	GraphicsView m_videoView;

	LayoutContent* mCurrentContent;
	LayoutContent* mNewContent;

	MainWnd::ViewMode m_mode;
};

#endif //layout_navigator_h_1340