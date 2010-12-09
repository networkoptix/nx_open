#ifndef serach_combobox_h2337
#define serach_combobox_h2337

#include <QComboBox>
#include <QTimer>

class CLSearchComboBox : public QComboBox
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
};

#endif // serach_combobox_h2337