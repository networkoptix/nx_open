#include "search_filter_item.h"

#include <QtGui/QHBoxLayout>
#include <QtGui/QStringListModel>
#include <QtGui/QToolButton>

#include "base/tagmanager.h"
#include "device/device_managmen/device_criteria.h"
#include "ui/video_cam_layout/layout_content.h"
#include "ui/graphicsview.h"
#include "ui/skin.h"

CLSearchEditCompleter::CLSearchEditCompleter(QObject *parent)
    : QCompleter(parent)
{
    setModel(new QStringListModel(this));
}

void CLSearchEditCompleter::filter(const QString &filter)
{
    // Do any filtering you like.
    // Here we just include all items that contain all words.
    QStringList filtered;

    foreach (const QString &word, filter.split(QLatin1Char(' '), QString::SkipEmptyParts))
    {
        foreach (const StringPair &item, m_list)
        {
            QString deviceTags = TagManager::objectTags(item.second).join(QLatin1String("\n"));
            if (item.first.contains(word, caseSensitivity()) || deviceTags.contains(word, caseSensitivity()))
                filtered.append(item.first);
        }
    }

    static_cast<QStringListModel *>(model())->setStringList(filtered);
    complete();
}

void CLSearchEditCompleter::updateStringPairs(const QList<StringPair> &list)
{
    m_list = list;
}


CLSearchEditItem::CLSearchEditItem(GraphicsView *view, LayoutContent *sceneContent, QWidget *parent)
    : QWidget(parent),
      m_view(view),
      m_sceneContent(sceneContent)
{
    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(m_lineEdit);

    setStyleSheet(QLatin1String("QWidget { background: transparent; }"));

    /*
    m_lineEdit->setStyleSheet(QLatin1String(
        "QLineEdit{ border-width: 2px; \n"
        "color: rgb(255, 255, 255); \n"
        "background-color: rgb(0, 16, 72);\n"
        "border-top-color: rgb(255, 255, 255); \n"
        "border-left-color: rgb(255, 255, 255); \n"
        "border-right-color: rgb(255, 255, 255); \n"
        "border-bottom-color: rgb(255, 255, 255); \n"
        "border-style: solid; \n"
        //"spacing: 22px; \n"
        //"margin-top: 3px; \n"
        //"margin-bottom: 3px; \n"
        //"padding: 16px; \n"
        "font-size: 25px; \n"
        "font-weight: normal; }\n"
        "QLineEdit:focus{ \n"
        "color: rgb(255, 255, 255);\n"
        "border-top-color: rgb(255, 255, 255);\n"
        "border-left-color: rgb(255, 255, 255); \n"
        "border-right-color: rgb(255, 255, 255); \n"
        "border-bottom-color: rgb(255, 255, 255);}"
    ));
    */

    m_lineEdit->setStyleSheet(QLatin1String(
        "QLineEdit{ border-width: 2px; \n"
        "color: rgb(150,150, 150); \n"
        "background-color: rgb(0, 16, 72);\n"
        "border-top-color: rgb(150,150, 150); \n"
        "border-left-color: rgb(150,150, 150); \n"
        "border-right-color: rgb(150,150, 150); \n"
        "border-bottom-color: rgb(150,150, 150); \n"
        "border-style: solid; \n"
        //"spacing: 22px; \n"
        //"margin-top: 3px; \n"
        //"margin-bottom: 3px; \n"
        //"padding: 16px; \n"
        "font-size: 25px; \n"
        "font-weight: normal; }\n"
        "QLineEdit:focus{ \n"
        "color: rgb(150,150, 150);\n"
        "border-top-color: rgb(150,150, 150);\n"
        "border-left-color: rgb(150,150, 150); \n"
        "border-right-color: rgb(150,150, 150); \n"
        "border-bottom-color: rgb(150,150, 150);}"
    ));

    m_completer = new CLSearchEditCompleter(m_lineEdit);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    m_completer->setMaxVisibleItems(10);

    QListView *listView = new QListView;

    listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    listView->setSelectionMode(QAbstractItemView::SingleSelection);

    listView->setStyleSheet(QLatin1String(
        "color: rgb(150, 150, 150);"
        "background-color: rgb(0, 16, 72);"
        "selection-color: yellow;"
        "selection-background-color: blue;"
        "font-size: 20px;"));

    m_completer->setPopup(listView);

    m_lineEdit->setCompleter(m_completer);

    connect(m_lineEdit, SIGNAL(textChanged(QString)), this, SLOT(onEditTextChanged(QString)));
    connect(m_completer, SIGNAL(activated(QString)), m_lineEdit, SLOT(selectAll()));

    mTimer.setSingleShot(true);
    mTimer.setInterval(600);
    connect(&mTimer, SIGNAL(timeout()), this, SLOT(onTimer()));

    QToolButton *liveButton = new QToolButton(this);
    liveButton->setText(tr("Live"));
    liveButton->setIcon(Skin::icon(QLatin1String("webcam.png")));
    liveButton->setIconSize(QSize(30, 30));

    connect(liveButton, SIGNAL(clicked()), this, SLOT(onLiveButtonClicked()));

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_lineEdit);
    mainLayout->addWidget(liveButton);
    setLayout(mainLayout);

    m_lineEdit->installEventFilter(this);
    m_lineEdit->setFocus();
}

CLSearchEditItem::~CLSearchEditItem()
{
}

QLineEdit *CLSearchEditItem::lineEdit() const
{
    return m_lineEdit;
}

bool CLSearchEditItem::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_lineEdit || event->type() != QEvent::KeyPress)
        return false;

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    keyEvent->accept();
    if (m_completer->popup()->isVisible())
    {
        // The following keys are forwarded by the completer to the widget
        switch (keyEvent->key())
        {
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            keyEvent->ignore();
            return false; // Let the completer do default behavior
        default:
            break;
        }
    }

    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
    {
        m_view->scene()->clearFocus();
        //keyEvent->ignore();
        if (!m_completer->popup()->isVisible())
        {
            m_completer->popup()->hide();
            return true;
        }
    }

    const bool isShortcut = (keyEvent->modifiers() & Qt::ControlModifier) && keyEvent->key() == Qt::Key_E;
    if (isShortcut)
        return true; // Don't send the shortcut (CTRL-E) to the text edit.

    const bool ctrlOrShift = keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    if (!ctrlOrShift && keyEvent->modifiers() != Qt::NoModifier)
    {
        m_completer->popup()->hide();
        return false;
    }

    m_completer->filter(m_lineEdit->text());
    //m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));

    return false;
}

void CLSearchEditItem::onEditTextChanged(const QString &text)
{
    m_view->onUserInput(true, true);

    mTimer.stop();
    mTimer.start();

    QList<QPair<QString, QString> > stringPairs;

    CLDeviceCriteria cr = m_sceneContent->getDeviceCriteria();
    cr.setCriteria(CLDeviceCriteria::FILTER);
    cr.setFilter(text);
    foreach (CLDevice* dev, CLDeviceManager::instance().getDeviceList(cr))
    {
        stringPairs.append(QPair<QString, QString>(dev->toString(), dev->getUniqueId()));
        dev->releaseRef();
    }

    m_completer->updateStringPairs(stringPairs);
    m_completer->filter(text);
}

void CLSearchEditItem::onTimer()
{
    const QString text = m_lineEdit->text();

    mTimer.stop();

    CLDeviceCriteria cr = m_sceneContent->getDeviceCriteria();
    if (text.length() >= 4)
    {
        cr.setCriteria(CLDeviceCriteria::FILTER);
        cr.setFilter(text);
    }
    else
    {
        cr.setCriteria(CLDeviceCriteria::NONE);
    }
    m_sceneContent->setDeviceCriteria(cr);
}

void CLSearchEditItem::onLiveButtonClicked()
{
    QString text = m_lineEdit->text().trimmed();
    if (text.compare(tr("live"), Qt::CaseInsensitive) != 0
        && !text.startsWith(tr("live+"), Qt::CaseInsensitive)
        && !text.endsWith(tr("+live"), Qt::CaseInsensitive)
        && !text.contains(tr("+live+"), Qt::CaseInsensitive))
    {
        if (!text.isEmpty())
            text += QLatin1Char('+');
        text += tr("live");
    }
    if (text != m_lineEdit->text())
        m_lineEdit->setText(text);
}
