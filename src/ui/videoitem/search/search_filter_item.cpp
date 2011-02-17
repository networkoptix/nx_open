#include "search_filter_item.h"

#include "search_edit.h"
#include "device\device_managmen\device_criteria.h"
#include "ui\video_cam_layout\layout_content.h"


CLSerachEditItem::CLSerachEditItem(QWidget* parent, LayoutContent* sceneContent):
QWidget(parent),
m_sceneContent(sceneContent),
m_width(100),
m_height(100)
{
	m_lineEdit = new CLSearchEdit(this);
	m_lineEdit->setFocus();

	
	QString style (

		"QLineEdit{ border-width: 2px; \n"
		"color: rgb(0, 240, 240); \n"
		"background-color: rgb(0, 16, 72);\n"
		"border-top-color: rgb(0, 240, 240); \n"
		"border-left-color: rgb(0, 240, 240); \n"
		"border-right-color: rgb(0, 240, 240); \n"
		"border-bottom-color: rgb(0, 240, 240); \n"
		"border-style: solid; \n"
		//"spacing: 22px; \n"
		//"margin-top: 3px; \n"
		//"margin-bottom: 3px; \n"
		//"padding: 16px; \n"
		"font-size: 25px; \n"
		"font-weight: normal; }\n"
		"QLineEdit:focus{ \n"
		"color: rgb(0, 240, 240);\n"
		"border-top-color: rgb(0, 240, 240);\n"
		"border-left-color: rgb(0, 240, 240); \n"
		"border-right-color: rgb(0, 240, 240); \n"
		"border-bottom-color: rgb(0, 240, 240);}"
		);


	m_lineEdit->setStyleSheet(style);
	

	m_height = 40;

	
	m_completer = new CLSerchEditCompleter(this);
	m_completer->setMaxVisibleItems(10);
	m_completer->setCompletionMode(QCompleter::PopupCompletion);
	m_completer->setCaseSensitivity(Qt::CaseInsensitive);

	QListView* lstview = new QListView();
	lstview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	lstview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);



	lstview->setStyleSheet(
		"color: rgb(0, 240, 240);"
		"background-color: rgb(0, 16, 72);"
		"selection-color: yellow;"
		"selection-background-color: blue;"
		"font-size: 20px; \n");

	m_completer->setPopup(lstview);



	m_lineEdit->setCompleter(m_completer);
	connect(m_lineEdit, SIGNAL( textChanged ( const QString &) ), this, SLOT(onEditTextChanged (const QString& )));


	mTimer.setSingleShot(true);
	mTimer.setInterval(600);
	connect(&mTimer, SIGNAL(timeout()), this, SLOT(onTimer()) );

	resize();

	setVisible(true);
}

CLSerachEditItem::~CLSerachEditItem()
{

}



void CLSerachEditItem::resize()
{
	int vpw = 800;
	if (parentWidget())
		vpw = parentWidget()->size().width();

	m_width = vpw/3;
	if (m_width < 300)
		m_width  = 300;

	//m_height = m_lineEdit->size().height();
	m_lineEdit->resize(m_width, m_height);

	move(QPoint(vpw/2 - m_width/2, 0));
	QWidget::resize(m_width, m_height);
	
}



void CLSerachEditItem::onEditTextChanged (const QString & text)
{
	mTimer.stop();
	mTimer.start();


	CLDeviceCriteria cr = m_sceneContent->getDeviceCriteria();
	cr.setCriteria(CLDeviceCriteria::FILTER);
	cr.setFilter(m_lineEdit->text());

	QStringList result;

	CLDeviceList all_devs =  CLDeviceManager::instance().getDeviceList(cr);
	foreach(CLDevice* dev, all_devs)
	{
		result << dev->toString();
		dev->releaseRef();
	}


	
	m_completer->updateStringLst(result);
	/**/

}

void CLSerachEditItem::onTimer()
{
	mTimer.stop();


	CLDeviceCriteria cr = m_sceneContent->getDeviceCriteria();
	cr.setCriteria(CLDeviceCriteria::FILTER);
	cr.setFilter(m_lineEdit->text());

	if (m_lineEdit->text().length()<4)
		cr.setFilter("39827fjhkdjfhurw98r7029r");

	m_sceneContent->setDeviceCriteria(cr);

}
