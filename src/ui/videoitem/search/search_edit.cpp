#include "search_edit.h"
#include "base/log.h"
#include "base/tagmanager.h"

CLSerchEditCompleter::CLSerchEditCompleter(QObject * parent):
QCompleter(parent)
{
	setModel(&m_model);
}

void CLSerchEditCompleter::filter(const QString &filter)
{
	// Do any filtering you like.
	// Here we just include all items that contain all words.
        QStringList filtered;

        typedef QPair<QString, QString> StringPair;

	foreach(const QString &word, filter.split(QLatin1Char(' '), QString::SkipEmptyParts))
        {
            foreach(const StringPair& item, m_list)
            {
                QString deviceTags = TagManager::objectTags(item.second).join("\n");
                if (item.first.contains(word, caseSensitivity()) || deviceTags.contains(word, caseSensitivity()))
                {
                    filtered.append(item.first);
                }
            }
        }

	m_model.setStringList(filtered);
	complete();
}

void CLSerchEditCompleter::updateStringPairs(const QList<QPair<QString, QString> >& lst)
{
	m_list = lst;
}

//=======================================================

CLSearchEdit::CLSearchEdit(QWidget *parent)
: QLineEdit(parent),
c(0),
mFocusWidget(0)
{
	setFocusPolicy(Qt::StrongFocus);
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


void CLSearchEdit::setFocusWidget(QWidget* fw)
{
	mFocusWidget = fw;
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

		}
	}

    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    {
        mFocusWidget->setFocus();
        //e->ignore();
        if (!c || !c->popup()->isVisible())
            return;
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

    //if (reason == Qt::TabFocusReason )          return;

    cl_log.log(QLatin1String("reason ==================================== "), reason, cl_logALWAYS);

    QLineEdit::focusInEvent(e);
}
