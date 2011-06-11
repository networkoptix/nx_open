#ifndef serach_edit_2215_h
#define serach_edit_2215_h 

class CLSerchEditCompleter : public QCompleter
{
	Q_OBJECT 
public:    
	CLSerchEditCompleter(QObject * parent);

	void filter(QString filter);

	void updateStringLst(QStringList lst);

private:    
	QStringList m_list;    
	QStringListModel m_model;    
	QString m_word;
}; 

class CLSearchEdit : public QLineEdit
{    
	Q_OBJECT 
public:    
	CLSearchEdit(QWidget *parent = 0);    
	~CLSearchEdit();     
	void setCompleter(CLSerchEditCompleter *c);
	CLSerchEditCompleter *completer() const; 

    void setViewPort(QWidget* vport);

protected:    
	void keyPressEvent(QKeyEvent *e); 
    void focusInEvent ( QFocusEvent * e );

	private slots:    
		void insertCompletion(const QString &completion); 

private:    
	CLSerchEditCompleter *c;

    QWidget* m_Vport;

}; 
#endif // serach_edit_2215_h
