#include "search_edit.h"


CLSearchComboBox::CLSearchComboBox( QWidget * parent ):
QComboBox(parent)
{
	connect(this, SIGNAL( editTextChanged ( const QString &) ), this, SLOT(onEditTextChanged (const QString& )));
	mTimer.setSingleShot(true);
	mTimer.setInterval(600);

	connect(&mTimer, SIGNAL(timeout()), this, SLOT(onTimer()) );
	setInsertPolicy(QComboBox::NoInsert);
}

CLSearchComboBox::~CLSearchComboBox()
{

}


void CLSearchComboBox::onEditTextChanged (const QString & text)
{
	mTimer.stop();
	mTimer.start();
}

void CLSearchComboBox::onTimer()
{
	mTimer.stop();

	if (currentText().length()>=4)
	{
		emit onTextChanged(currentText());
	}
}