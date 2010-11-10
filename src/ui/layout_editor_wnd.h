#ifndef layout_editor_wnd_h1223
#define layout_editor_wnd_h1223

#include <QDialog>

class CLLayoutNavigator;
class LayoutContent;

class CLLayoutEditorWnd : public QDialog
{
	Q_OBJECT

public:
	CLLayoutEditorWnd(LayoutContent* contexttoEdit);
	~CLLayoutEditorWnd();
private:


private slots:
	void onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*);
private:
	void closeEvent ( QCloseEvent * event );
	void resizeEvent ( QResizeEvent * event);
	void destroyNavigator(CLLayoutNavigator*& nav);
private:

	CLLayoutNavigator* m_topView;
	CLLayoutNavigator* m_bottomView;
	CLLayoutNavigator* m_editedView;

};

/**/


#endif // layout_editor_wnd_h1223