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
      m_handler(handler),
      m_param(obj),
      m_widget(0),
      m_layout(new QHBoxLayout()),
      m_hint(hint)
{
    parent.addWidget(*this);
    setLayout(m_layout);
    setToolTip(m_hint);

    QObject::connect(this, SIGNAL( setAdvancedParam(const CameraSetting&) ),
        handler, SLOT( setAdvancedParam(const CameraSetting&) )  );
}

QnAbstractSettingsWidget::~QnAbstractSettingsWidget()
{

}

const CameraSetting& QnAbstractSettingsWidget::param() const
{
    return m_param;
}

QWidget* QnAbstractSettingsWidget::toWidget()
{
    return m_widget;
}

void QnAbstractSettingsWidget::setParam(const CameraSettingValue& val)
{
    m_param.setCurrent(val);
    emit setAdvancedParam(m_param);
}

//==============================================
QnSettingsOnOffWidget::QnSettingsOnOffWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent):
    QnAbstractSettingsWidget(handler, obj, parent, obj.getDescription())
{
    m_checkBox = new QCheckBox(m_param.getName());

    //mlayout->addWidget(new QWidget());
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addWidget(m_checkBox);
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    //mlayout->addWidget(new QWidget());

    if (m_param.getCurrent() == m_param.getMax())
        m_checkBox->setCheckState(Qt::Checked);

    connect(m_checkBox, SIGNAL(stateChanged ( int )), this, SLOT(stateChanged(int)));

    //QPalette plt;	plt.setColor(QPalette::WindowText, Qt::white);	checkBox->setPalette(plt);//black

    m_widget = m_checkBox;
    //setMinimumSize(m_checkBox->sizeHint());
}

void QnSettingsOnOffWidget::refresh()
{
    if (m_param.getCurrent() == m_param.getMax()) {
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

    QString val = state == Qt::Checked ? (QString)m_param.getMax() : (QString)m_param.getMin();
    setParam(val);
}

void QnSettingsOnOffWidget::updateParam(QString val)
{
    m_checkBox->setChecked(val == m_param.getMax()); // emits stateChanged
}

//==============================================
QnSettingsMinMaxStepWidget::QnSettingsMinMaxStepWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent):
    QnAbstractSettingsWidget(handler, obj, parent, obj.getDescription())
{
    QVBoxLayout *vlayout = new QVBoxLayout();
    //mlayout->addWidget(new QWidget());
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addLayout(vlayout);
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    //mlayout->addWidget(new QWidget());

    m_groupBox = new QGroupBox();
    vlayout->addWidget(new QWidget());
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(m_groupBox);
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(new QWidget());

    m_slider = new QnSettingsSlider(Qt::Horizontal, m_groupBox);
    //m_slider->setMinimumWidth(130);
    //m_slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    //QFont font("Courier New", 11);
    //groupBox->setFont(font);

    m_slider->setRange(m_param.getMin(), m_param.getMax());
    m_slider->setValue(m_param.getCurrent());
    m_slider->setWindowTitle(m_param.getName() + QLatin1String(" (") + (QString)m_param.getCurrent() + QLatin1Char(')'));

    m_groupBox->setTitle(m_param.getName() + QLatin1String(" (") + (QString)m_param.getCurrent() + QLatin1Char(')'));

    QVBoxLayout *layout = new QVBoxLayout(m_groupBox);
    layout->addWidget(m_slider);

    connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onValChanged()));
    connect(m_slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValChanged(int)) );

    m_widget = m_groupBox;
    //setMinimumSize(groupBox->sizeHint());
}

void QnSettingsMinMaxStepWidget::refresh()
{
    m_slider->setRange(m_param.getMin(), m_param.getMax());
    m_slider->setValue(m_param.getCurrent());
    m_slider->setWindowTitle(m_param.getName() + QLatin1String(" (") + (QString)m_param.getCurrent() + QLatin1Char(')'));

    static_cast<QGroupBox*>(m_widget)->setTitle(m_param.getName() + QLatin1String(" (") + (QString)m_param.getCurrent() + QLatin1Char(')'));
}

void QnSettingsMinMaxStepWidget::onValChanged(int val)
{
    m_groupBox->setTitle(m_param.getName() + QLatin1Char('(') + QString::number(val) + QLatin1Char(')'));
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
    m_layout->addLayout(vlayout);
    //mlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    //mlayout->addWidget(new QWidget());

    QGroupBox* groupBox = new QGroupBox();
    vlayout->addWidget(new QWidget());
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(groupBox);
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(new QWidget());

    //groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    groupBox->setTitle(m_param.getName());
    //groupBox->setMinimumWidth(140);

    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    QStringList values = static_cast<QString>(m_param.getMin()).split(QLatin1Char(','));
    for (int i = 0; i < values.length(); ++i)
    {
        QString val = values[i].trimmed();

        QRadioButton *btn = new QRadioButton(val, groupBox);
        layout->addWidget(btn);

        if (val == m_param.getCurrent())
            btn->setChecked(true);

        btn->setObjectName(val);
        //btn->setFont(settings_font);
    
        m_radioBtns.push_back(btn);

        connect(btn , SIGNAL(clicked()), this, SLOT(onClicked()));
    }

    m_widget = groupBox;
    //setMinimumSize(groupBox->sizeHint());
}

void QnSettingsEnumerationWidget::refresh()
{
    QStringList values = static_cast<QString>(m_param.getMin()).split(QLatin1Char(','));
    for (int i = 0; i < values.length(); ++i)
    {
        QString val = values[i].trimmed();
        QRadioButton *btn = m_radioBtns.at(i);

        if (val == m_param.getCurrent()) {
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
    QnAbstractSettingsWidget(handler, m_dummyVal, parent, obj.getDescription()), //ToDo: remove ugly hack: dummyVal potentially can be used before instantiation
    m_dummyVal(obj)
{
    QPushButton* btn = new QPushButton(m_param.getName());
    m_layout->addWidget(new QWidget());
    m_layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addWidget(btn);
    m_layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addWidget(new QWidget()); // TODO: hueta

    QObject::connect(btn, SIGNAL(released()), this, SLOT(onClicked()));

    btn->setFocusPolicy(Qt::NoFocus);

    m_widget = btn;
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

    m_layout->addWidget(new QWidget());
    m_layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addWidget(lineEdit);
    m_layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addWidget(new QLabel(obj.getName()));

    connect(lineEdit, SIGNAL(editingFinished()), this, SLOT(onChange()));

    m_widget = lineEdit;
}

void QnSettingsTextFieldWidget::onChange()
{
    setParam(static_cast<QLineEdit*>(m_widget)->text());
}

void QnSettingsTextFieldWidget::refresh()
{
    static_cast<QLineEdit*>(m_widget)->setText(m_param.getCurrent());
}

void QnSettingsTextFieldWidget::updateParam(QString val)
{

}

//==============================================
QnSettingsControlButtonsPairWidget::QnSettingsControlButtonsPairWidget(QObject* handler, CameraSetting& obj, QnSettingsScrollArea& parent):
    QnAbstractSettingsWidget(handler, obj, parent, obj.getDescription())
{
    QHBoxLayout *hlayout = new QHBoxLayout();
    m_layout->addLayout(hlayout);

    m_minBtn = new QPushButton(QString::fromLatin1("-"));
    m_minBtn->setFocusPolicy(Qt::NoFocus);
    m_maxBtn = new QPushButton(QString::fromLatin1("+"));
    m_maxBtn->setFocusPolicy(Qt::NoFocus);

    hlayout->addStretch(1);
    hlayout->addWidget(new QLabel(obj.getName()));
    hlayout->addWidget(m_minBtn);
    hlayout->addWidget(m_maxBtn);
    hlayout->addStretch(1);

    QObject::connect(m_minBtn, SIGNAL(pressed()), this, SLOT(onMinPressed()));
    QObject::connect(m_minBtn, SIGNAL(released()), this, SLOT(onMinReleased()));

    QObject::connect(m_maxBtn, SIGNAL(pressed()), this, SLOT(onMaxPressed()));
    QObject::connect(m_maxBtn, SIGNAL(released()), this, SLOT(onMaxReleased()));

    m_widget = 0;
}

void QnSettingsControlButtonsPairWidget::onMinPressed()
{
    setParam(m_param.getMin());
}

void QnSettingsControlButtonsPairWidget::onMinReleased()
{
    setParam(m_param.getStep());
}

void QnSettingsControlButtonsPairWidget::onMaxPressed()
{
    setParam(m_param.getMax());
}

void QnSettingsControlButtonsPairWidget::onMaxReleased()
{
    setParam(m_param.getStep());
}

void QnSettingsControlButtonsPairWidget::refresh()
{
    
}

void QnSettingsControlButtonsPairWidget::updateParam(QString val)
{
    
}
