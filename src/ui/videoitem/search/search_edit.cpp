#include "search_edit.h"

CLSerchEditCompleter::CLSerchEditCompleter(QObject * parent):
QCompleter(parent)
{
	setModel(&m_model);
}     

void CLSerchEditCompleter::filter(QString filter)    
{        
	// Do any filtering you like.        
	// Here we just include all items that contain all words.
	QStringList filtered = m_list;//.filter(word, caseSensitivity());

	QStringList words = filter.split(" ", QString::SkipEmptyParts);

	foreach(QString word, words)
	{
		filtered = filtered.filter(word, caseSensitivity());
	}

	m_model.setStringList(filtered);        
	complete();    
}     

void CLSerchEditCompleter::updateStringLst(QStringList lst)
{
	m_list = lst;
}

//=======================================================

CLSearchEdit::CLSearchEdit(QWidget *parent)
: QLineEdit(parent), 
c(0),
m_Vport(0)
{
}

CLSearchEdit::~CLSearchEdit()
{
}

void CLSearchEdit::setCompleter(CLSerchEditCompleter *completer)
{
	if (c)
		QObject::disconnect(c, 0, this, 0);

	c = completer;

	if (!c)
		return;

	c->setWidget(this);
	connect(completer, SIGNAL(activated(const QString&)), this, SLOT(insertCompletion(const QString&)));
}

CLSerchEditCompleter *CLSearchEdit::completer() const
{
	return c;
}

void CLSearchEdit::setViewPort(QWidget* vport)
{
    m_Vport = vport;
}

void CLSearchEdit::insertCompletion(const QString& completion)
{
	setText(completion);
	selectAll();
}

void CLSearchEdit::keyPressEvent(QKeyEvent *e)
{
	if (c && c->popup()->isVisible())
	{
		// The following keys are forwarded by the completer to the widget
		switch (e->key())
		{
		
		case Qt::Key_Escape:
		case Qt::Key_Tab:
		case Qt::Key_Backtab:
			e->ignore();
			return; // Let the completer do default behavior

        case Qt::Key_Return:
        case Qt::Key_Enter:
            e->ignore();
            clearFocus();
            if (m_Vport)
                m_Vport->setFocus();
            return; // Let the completer do default behavior


		}
	}

    if (e->key()==Qt::Key_Return && e->key()==Qt::Key_Enter)
    {
        clearFocus();
        if (m_Vport)
            m_Vport->setFocus();
    }

	bool isShortcut = (e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_E;
	if (!isShortcut)
		QLineEdit::keyPressEvent(e); // Don't send the shortcut (CTRL-E) to the text edit.

	if (!c)
		return;

	bool ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
	if (!isShortcut && !ctrlOrShift && e->modifiers() != Qt::NoModifier)
	{
		c->popup()->hide();
		return;
	}

	c->filter(text());
	//c->popup()->setCurrentIndex(c->completionModel()->index(0, 0));
}


void CLSearchEdit::focusInEvent ( QFocusEvent * e )
{
    Qt::FocusReason reason = e->reason();

    if (reason == Qt::TabFocusReason )
        return;

    QLineEdit::focusInEvent(e);
}