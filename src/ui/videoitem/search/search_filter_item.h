#ifndef serach_edit_item_h_2017
#define serach_edit_item_h_2017

#include <QWidget>
#include <QTimer>
class LayoutContent;

class CLSearchEdit;
class CLSerchEditCompleter;

class CLSerachEditItem : public QWidget
{
	Q_OBJECT
public:
	CLSerachEditItem(QWidget* parent, LayoutContent* sceneContent);
	~CLSerachEditItem();
	void resize();

	
protected slots:
	void onEditTextChanged (const QString & text);
	void onTimer(); 


	
protected:
	LayoutContent* m_sceneContent;

	int m_width;
	int m_height;

	CLSearchEdit* m_lineEdit;
	CLSerchEditCompleter* m_completer;
	QTimer mTimer;

	

};

#endif //serach_edit_item_h_2017