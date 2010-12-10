#ifndef serach_combobox_h2337
#define serach_combobox_h2337

#include <QLineEdit>
#include <QTimer>
class QCompleter;

class CLSearchComboBox : public QLineEdit
{
	Q_OBJECT
public:
	CLSearchComboBox( QWidget * parent = 0 );
	~CLSearchComboBox();

signals:
	void onTextChanged(QString text) ;

protected slots:
	void onEditTextChanged (const QString & text);
	void onTimer(); 
protected:
	QTimer mTimer;
	QCompleter* m_completer;
};

#endif // serach_combobox_h2337