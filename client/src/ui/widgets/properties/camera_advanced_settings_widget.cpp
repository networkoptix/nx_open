#include "camera_advanced_settings_widget.h"

#include <QtGui/QApplication>
#include <QtGui/QStyle>
#include <QtGui/QStyleFactory>
#include <QtGui/QVBoxLayout>


QnSettingsScrollArea::QnSettingsScrollArea(QWidget* parent):
    QScrollArea(parent)
{
    QWidget *widget = new QWidget();
    widget->setLayout(new QVBoxLayout());
    setWidget(widget);
    if(isVisible())
        widget->show();

    setWidgetResizable(true);
}

void QnSettingsScrollArea::addWidget(QWidget& widgetToAdd)
{
    if (widget()->layout()->count() != 0)
        widget()->layout()->removeItem(widget()->layout()->itemAt(widget()->layout()->count() - 1));

    widgetToAdd.setParent(widget());
    dynamic_cast<QVBoxLayout*>(widget()->layout())->addWidget(&widgetToAdd);
    static_cast<QVBoxLayout*>(widget()->layout())->addStretch(1);
}

//==============================================

QnSettingsSlider::QnSettingsSlider(Qt::Orientation orientation, QWidget *parent)
: QSlider(orientation, parent)
{
    //setStyle(QStyleFactory::create(qApp->style()->objectName()));
}

void QnSettingsSlider::keyReleaseEvent(QKeyEvent *event)
{
    QSlider::keyReleaseEvent(event);

    emit onKeyReleased();
}
//==============================================

QnAbstractSettingsWidget::QnAbstractSettingsWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea &parent, const QString& hint)
    : QWidget(0),
      mHandler(handler),
      mParam(obj),
      mWidget(0),
      mlayout(new QHBoxLayout()),
      m_hint(hint)
{
    parent.addWidget(*this);
    setLayout(mlayout);
    setToolTip(m_hint);

    QObject::connect(this, SIGNAL( setAdvancedParam(const CameraSetting&) ),
        handler, SLOT( setAdvancedParam(const CameraSetting&) )  );
}

QnAbstractSettingsWidget::~QnAbstractSettingsWidget()
{

}

const CameraSetting& QnAbstractSettingsWidget::param() const
{
    return mParam;
}

QWidget* QnAbstractSettingsWidget::toWidget()
{
    return mWidget;
}

void QnAbstractSettingsWidget::setParam(const CameraSettingValue& val)
{
    mParam.setCurrent(val);
    emit setAdvancedParam(mParam);
}

//==============================================
QnSettingsOnOffWidget::QnSettingsOnOffWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent):
    QnAbstractSettingsWidget(handler, obj, parent, obj.getDescription())
{
    m_checkBox = new QCheckBox(mParam.getName());

    //mlayout->addWidget(new QWidget());
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    mlayout->addWidget(m_checkBox);
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    //mlayout->addWidget(new QWidget());

    if (mParam.getCurrent() == mParam.getMax())
        m_checkBox->setCheckState(Qt::Checked);

    connect(m_checkBox, SIGNAL(stateChanged ( int )), this, SLOT(stateChanged(int)));

    //QPalette plt;	plt.setColor(QPalette::WindowText, Qt::white);	checkBox->setPalette(plt);//black

    mWidget = m_checkBox;
    //setMinimumSize(m_checkBox->sizeHint());
}

void QnSettingsOnOffWidget::refresh()
{
    if (mParam.getCurrent() == mParam.getMax()) {
        m_checkBox->setCheckState(Qt::Checked);
    } else {
        m_checkBox->setCheckState(Qt::Unchecked);
    }

}

QnSettingsOnOffWidget::~QnSettingsOnOffWidget()
{
}

void QnSettingsOnOffWidget::stateChanged(int state)
{
    //if (mParam.value.possible_values.count()<2)
    //{
    //    cl_log.log(QLatin1String("param.value.possible_values.count()<2 !!!!"), cl_logERROR);
    //    return;
    //}

    QString val = state == Qt::Checked ? (QString)mParam.getMax() : (QString)mParam.getMin();
    setParam(val);
}

void QnSettingsOnOffWidget::updateParam(QString val)
{
    m_checkBox->setChecked(val == mParam.getMax()); // emits stateChanged
}

//==============================================
QnSettingsMinMaxStepWidget::QnSettingsMinMaxStepWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent):
    QnAbstractSettingsWidget(handler, obj, parent, obj.getDescription())
{
    QVBoxLayout *vlayout = new QVBoxLayout();
    //mlayout->addWidget(new QWidget());
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    mlayout->addLayout(vlayout);
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    //mlayout->addWidget(new QWidget());

    groupBox = new QGroupBox();
    vlayout->addWidget(new QWidget());
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(groupBox);
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(new QWidget());

    m_slider = new QnSettingsSlider(Qt::Horizontal, groupBox);
    //m_slider->setMinimumWidth(130);
    //m_slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    //QFont font("Courier New", 11);
    //groupBox->setFont(font);

    m_slider->setRange(mParam.getMin(), mParam.getMax());
    m_slider->setValue(mParam.getCurrent());
    m_slider->setWindowTitle(mParam.getName() + QLatin1String(" (") + (QString)mParam.getCurrent() + QLatin1Char(')'));

    groupBox->setTitle(mParam.getName() + QLatin1String(" (") + (QString)mParam.getCurrent() + QLatin1Char(')'));

    QVBoxLayout *layout = new QVBoxLayout(groupBox);
    layout->addWidget(m_slider);

    connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onValChanged()));
    connect(m_slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValChanged(int)) );

    mWidget = groupBox;
    //setMinimumSize(groupBox->sizeHint());
}

void QnSettingsMinMaxStepWidget::refresh()
{
    m_slider->setRange(mParam.getMin(), mParam.getMax());
    m_slider->setValue(mParam.getCurrent());
    m_slider->setWindowTitle(mParam.getName() + QLatin1String(" (") + (QString)mParam.getCurrent() + QLatin1Char(')'));

    static_cast<QGroupBox*>(mWidget)->setTitle(mParam.getName() + QLatin1String(" (") + (QString)mParam.getCurrent() + QLatin1Char(')'));
}

void QnSettingsMinMaxStepWidget::onValChanged(int val)
{
    groupBox->setTitle(mParam.getName() + QLatin1Char('(') + QString::number(val) + QLatin1Char(')'));
}

void QnSettingsMinMaxStepWidget::onValChanged()
{
    setParam(m_slider->value());
}

void QnSettingsMinMaxStepWidget::updateParam(QString val)
{
    CameraSettingValue cv(val);
    m_slider->setValue(cv);
}

//==============================================
QnSettingsEnumerationWidget::QnSettingsEnumerationWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent):
    QnAbstractSettingsWidget(handler, obj, parent, obj.getDescription())
{
    QVBoxLayout *vlayout = new QVBoxLayout();
    //mlayout->addWidget(new QWidget());
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    mlayout->addLayout(vlayout);
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    //mlayout->addWidget(new QWidget());

    QGroupBox* groupBox = new QGroupBox();
    vlayout->addWidget(new QWidget());
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(groupBox);
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(new QWidget());

    //groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    groupBox->setTitle(mParam.getName());
    //groupBox->setMinimumWidth(140);

    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    QStringList values = static_cast<QString>(mParam.getMin()).split(QLatin1Char(','));
    for (int i = 0; i < values.length(); ++i)
    {
        QString val = values[i].trimmed();

        QRadioButton *btn = new QRadioButton(val, groupBox);
        layout->addWidget(btn);

        if (val == mParam.getCurrent())
            btn->setChecked(true);

        btn->setObjectName(val);
        //btn->setFont(settings_font);
    
        m_radioBtns.push_back(btn);

        connect(btn , SIGNAL(clicked()), this, SLOT(onClicked()));
    }

    mWidget = groupBox;
    //setMinimumSize(groupBox->sizeHint());
}

void QnSettingsEnumerationWidget::refresh()
{
    QStringList values = static_cast<QString>(mParam.getMin()).split(QLatin1Char(','));
    for (int i = 0; i < values.length(); ++i)
    {
        QString val = values[i].trimmed();
        QRadioButton *btn = m_radioBtns.at(i);

        if (val == mParam.getCurrent()) {
            btn->setChecked(true);
        } else {
            btn->setChecked(false);
        }
    }
}

void QnSettingsEnumerationWidget::onClicked()
{
    QString val = QObject::sender()->objectName();

    setParam(val);
}

void QnSettingsEnumerationWidget::updateParam(QString val)
{
    QRadioButton* btn = getBtnByname(val);
    if (btn)
        btn->setChecked(true);
}

QRadioButton* QnSettingsEnumerationWidget::getBtnByname(const QString& name)
{
    foreach(QRadioButton* btn, m_radioBtns)
    {
        if (btn->objectName() == name)
            return btn;
    }

    return 0;
}

//==================================================
QnSettingsButtonWidget::QnSettingsButtonWidget(QObject* handler, const CameraSetting& obj, QnSettingsScrollArea& parent):
    QnAbstractSettingsWidget(handler, dummyVal, parent, obj.getDescription()), //ToDo: remove ugly hack: dummyVal potentially can be used before instantiation
    dummyVal(obj)
{
    QPushButton* btn = new QPushButton(mParam.getName());
    mlayout->addWidget(new QWidget());
    mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    mlayout->addWidget(btn);
    mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    mlayout->addWidget(new QWidget()); // TODO: hueta

    QObject::connect(btn, SIGNAL(released()), this, SLOT(onClicked()));

    btn->setFocusPolicy(Qt::NoFocus);

    mWidget = btn;
    //setMinimumSize(btn->sizeHint());
}

void QnSettingsButtonWidget::refresh()
{
}

void QnSettingsButtonWidget::onClicked()
{
    setParam("");
}

void QnSettingsButtonWidget::updateParam(QString /*val*/)
{
    //cl_log.log("updateParam", cl_logALWAYS);
}

//==============================================
QnSettingsTextFieldWidget::QnSettingsTextFieldWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent):
    QnAbstractSettingsWidget(handler, obj, parent, obj.getDescription())
{
    QLineEdit *lineEdit = new QLineEdit();
    lineEdit->setText(obj.getCurrent());

    mlayout->addWidget(new QWidget());
    mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    mlayout->addWidget(lineEdit);
    mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    mlayout->addWidget(new QLabel(obj.getName()));

    connect(lineEdit, SIGNAL(editingFinished()), this, SLOT(onChange()));

    mWidget = lineEdit;
}

void QnSettingsTextFieldWidget::onChange()
{
    setParam(static_cast<QLineEdit*>(mWidget)->text());
}

void QnSettingsTextFieldWidget::refresh()
{
    static_cast<QLineEdit*>(mWidget)->setText(mParam.getCurrent());
}

void QnSettingsTextFieldWidget::updateParam(QString val)
{

}

//==============================================
QnSettingsControlButtonsPairWidget::QnSettingsControlButtonsPairWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent):
    QnAbstractSettingsWidget(handler, obj, parent, obj.getDescription())
{
    QHBoxLayout *hlayout = new QHBoxLayout();
    mlayout->addLayout(hlayout);

    minBtn = new QPushButton(QString::fromLatin1("-"));
    minBtn->setFocusPolicy(Qt::NoFocus);
    maxBtn = new QPushButton(QString::fromLatin1("+"));
    maxBtn->setFocusPolicy(Qt::NoFocus);

    hlayout->addStretch(1);
    hlayout->addWidget(new QLabel(obj.getName()));
    hlayout->addWidget(minBtn);
    hlayout->addWidget(maxBtn);
    hlayout->addStretch(1);

    QObject::connect(minBtn, SIGNAL(pressed()), this, SLOT(onMinPressed()));
    QObject::connect(minBtn, SIGNAL(released()), this, SLOT(onMinReleased()));

    QObject::connect(maxBtn, SIGNAL(pressed()), this, SLOT(onMaxPressed()));
    QObject::connect(maxBtn, SIGNAL(released()), this, SLOT(onMaxReleased()));

    mWidget = 0;
}

void QnSettingsControlButtonsPairWidget::onMinPressed()
{
    setParam(mParam.getMin());
}

void QnSettingsControlButtonsPairWidget::onMinReleased()
{
    setParam(mParam.getStep());
}

void QnSettingsControlButtonsPairWidget::onMaxPressed()
{
    setParam(mParam.getMax());
}

void QnSettingsControlButtonsPairWidget::onMaxReleased()
{
    setParam(mParam.getStep());
}

void QnSettingsControlButtonsPairWidget::refresh()
{
    
}

void QnSettingsControlButtonsPairWidget::updateParam(QString val)
{
    
}
