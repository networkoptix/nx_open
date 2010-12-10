#include "search_edit.h"

#include <QCompleter>
#include <QListView>

CLSearchComboBox::CLSearchComboBox( QWidget * parent ):
QLineEdit(parent)
{
	connect(this, SIGNAL( textChanged ( const QString &) ), this, SLOT(onEditTextChanged (const QString& )));
	mTimer.setSingleShot(true);
	mTimer.setInterval(600);

	connect(&mTimer, SIGNAL(timeout()), this, SLOT(onTimer()) );


	QStringList wordList;
	wordList << "alpha" << "omega" << "omicron" << "zeta";
	m_completer = new QCompleter(wordList, this);
	m_completer->setCompletionMode(QCompleter::PopupCompletion);
	m_completer->setCaseSensitivity(Qt::CaseInsensitive);

	//QListView* lst = new QListView(0);
	//m_completer->setPopup(lst);


	setCompleter(m_completer);

	

}

CLSearchComboBox::~CLSearchComboBox()
{

}


void CLSearchComboBox::onEditTextChanged (const QString & text)
{
	mTimer.stop();
	mTimer.start();

	m_completer->popup()->move(500,500);
	m_completer->popup()->setVisible(true);

}

void CLSearchComboBox::onTimer()
{
	mTimer.stop();

	if (text().length()>=4)
	{
		emit onTextChanged(text());
	}
}