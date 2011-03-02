#ifndef serach_edit_item_h_2017
#define serach_edit_item_h_2017

class LayoutContent;

class CLSearchEdit;
class CLSerchEditCompleter;
class GraphicsView;

class CLSerachEditItem : public QWidget
{
	Q_OBJECT
public:
	CLSerachEditItem(GraphicsView* view, QWidget* parent, LayoutContent* sceneContent);
	~CLSerachEditItem();
	void resize();

    void setVisible(bool visible);

    bool hasFocus() const;


protected slots:
	void onEditTextChanged (const QString & text);
	void onTimer(); 

protected:    
	
protected:
	LayoutContent* m_sceneContent;

	int m_width;
	int m_height;

	CLSearchEdit* m_lineEdit;
	CLSerchEditCompleter* m_completer;
    QListView* m_lstview;
	QTimer mTimer;

    GraphicsView* m_view;

	

};

#endif //serach_edit_item_h_2017