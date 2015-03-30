
#include "search_bookmarks_dialog.h"

#include <core/resource/camera_bookmark_fwd.h>

#include <ui/models/search_bookmarks_model.h>
#include <ui/dialogs/resource_selection_dialog.h>

#include "ui_search_bookmarks_dialog.h"

class QnSearchBookmarksDialog::Impl : public QObject
{
public:
    Impl(QDialog *owner);

    ~Impl();

    void updateHeadersWidth();

    void refresh();

private:
    void chooseCamera();


private:
    typedef QScopedPointer<Ui::BookmarksLog> UiImpl;
    
    QDialog * const m_owner;
    const UiImpl m_ui;
    QnSearchBookmarksModel * const m_model;

    QnResourceList m_cameras;
};

///

QnSearchBookmarksDialog::Impl::Impl(QDialog *owner)
    : m_owner(owner)
    , m_ui(new Ui::BookmarksLog())
    , m_model(new QnSearchBookmarksModel(this))

    , m_cameras()
{
    m_ui->setupUi(m_owner);

    const QDate now = QDateTime::currentDateTime().date();
    m_ui->dateEditFrom->setDate(now);
    m_ui->dateEditTo->setDate(now);
    m_ui->gridBookmarks->setModel(m_model);

    m_model->setDates(m_ui->dateEditFrom->date(), m_ui->dateEditTo->date());
    connect(m_ui->filterLineEdit, &QnSearchLineEdit::enterKeyPressed, this
        , [this]() 
    { 
        m_model->setFilterText(m_ui->filterLineEdit->lineEdit()->text()); 
        m_model->applyFilter(false);
    });
    connect(m_ui->filterLineEdit, &QnSearchLineEdit::escKeyPressed, this, [this]() 
    {
        m_ui->filterLineEdit->lineEdit()->setText(QString());
        m_model->setFilterText(QString());
        m_model->applyFilter(false);
    });

    connect(m_ui->dateEditFrom, &QDateEdit::userDateChanged, this, [this](const QDate &date) 
    { 
        m_model->setDates(date, m_ui->dateEditTo->date()); 
        m_model->applyFilter(false);
    });
    connect(m_ui->dateEditTo, &QDateEdit::userDateChanged, this, [this](const QDate &date) 
    { 
        m_model->setDates(m_ui->dateEditFrom->date(), date); 
        m_model->applyFilter(false);
    });
    
    connect(m_ui->refreshButton, &QPushButton::clicked, this, &Impl::refresh);
    connect(m_ui->cameraButton, &QPushButton::clicked, this, &Impl::chooseCamera);

    m_model->applyFilter(true);
}

QnSearchBookmarksDialog::Impl::~Impl()
{
}

void QnSearchBookmarksDialog::Impl::updateHeadersWidth()
{
    enum 
    {
        kNamePart = 8
        , kStartTimePart = 3
        , kLengthPart = 3
        , kTagsPart = 4
        , kCameraPart = 3
        , kPartsTotalNumber = kNamePart + kStartTimePart + kLengthPart + kTagsPart + kCameraPart
    };

    QHeaderView * const header = m_ui->gridBookmarks->horizontalHeader();
    const int totalWidth = header->width();
    header->resizeSection(QnSearchBookmarksModel::kName, totalWidth * kNamePart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kStartTime, totalWidth * kStartTimePart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kLength, totalWidth * kLengthPart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kTags, totalWidth * kTagsPart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kCamera, totalWidth * kCameraPart / kPartsTotalNumber);
}

void QnSearchBookmarksDialog::Impl::refresh()
{
    m_model->applyFilter(true);
}

void QnSearchBookmarksDialog::Impl::chooseCamera()
{
    QnResourceSelectionDialog dialog(m_owner);
    dialog.setSelectedResources(m_cameras);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_cameras = dialog.selectedResources();
        m_model->setCameras(m_cameras);
        m_model->applyFilter(false);

        static const QString kEmptyCamerasCaption = tr("<Any camera>");
        static const QString kCamerasTemplate = tr("camera(s)");
        static const QString kCamerasCaptionTemplate = lit("<%1 %2>");
        m_ui->cameraButton->setText(m_cameras.empty() ? kEmptyCamerasCaption : 
            kCamerasCaptionTemplate.arg(QString::number(m_cameras.size()), kCamerasTemplate));
    }
}

///

QnSearchBookmarksDialog::QnSearchBookmarksDialog(QWidget *parent)
    : QnButtonBoxDialog(parent)
    , m_impl(new Impl(this))
{
}

QnSearchBookmarksDialog::~QnSearchBookmarksDialog()
{
}

void QnSearchBookmarksDialog::resizeEvent(QResizeEvent *event)
{
    Base::resizeEvent(event);
    m_impl->updateHeadersWidth();
}

void QnSearchBookmarksDialog::showEvent(QShowEvent *event)
{
    Base::showEvent(event);
    m_impl->refresh();
}
