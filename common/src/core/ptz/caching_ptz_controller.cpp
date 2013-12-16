#include "caching_ptz_controller.h"

QnCachingPtzController::QnCachingPtzController(const QnPtzControllerPtr &baseController):
	base_type(baseController)
{
	connect(baseController.data(), &QnAbstractPtzController::finished, this, &QnCachingPtzController::at_baseController_finished);

	synchronize(Qn::AllPtzFields);
}

QnCachingPtzController::~QnCachingPtzController() {
	return;
}

bool QnCachingPtzController::extends(const QnPtzControllerPtr &baseController) {
	return 
        baseController->hasCapabilities(Qn::AsynchronousPtzCapability) &&
		!baseController->hasCapabilities(Qn::SynchronizedPtzCapability);
}

Qn::PtzCapabilities QnCachingPtzController::getCapabilities() {
	return baseController()->getCapabilities() | Qn::SynchronizedPtzCapability;
}

bool QnCachingPtzController::continuousMove(const QVector3D &speed) {
	if(!base_type::continuousMove(speed))
		return false;

	QMutexLocker locker(&m_mutex);
	m_data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
	return true;
}

bool QnCachingPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(!base_type::absoluteMove(space, position, speed))
		return false;

    QMutexLocker locker(&m_mutex);
    m_data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
	return true;
}

bool QnCachingPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!base_type::viewportMove(aspectRatio, viewport, speed))
		return false;

    QMutexLocker locker(&m_mutex);
    m_data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
	return true;
}

bool QnCachingPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
	if(space == Qn::DevicePtzCoordinateSpace) {
        QMutexLocker locker(&m_mutex);
		if(m_data.fields & Qn::DevicePositionPtzField) {
			*position = m_data.devicePosition;
			return true;
        } else {
			return false;
        }
    } else {
        QMutexLocker locker(&m_mutex);
        if(m_data.fields & Qn::LogicalPositionPtzField) {
            *position = m_data.logicalPosition;
            return true;
        } else {
            return false;
        }
    }
}

bool QnCachingPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    if(space == Qn::DevicePtzCoordinateSpace) {
        QMutexLocker locker(&m_mutex);
        if(m_data.fields & Qn::DeviceLimitsPtzField) {
            *limits = m_data.deviceLimits;
            return true;
        } else {
            return false;
        }
    } else {
        QMutexLocker locker(&m_mutex);
        if(m_data.fields & Qn::LogicalLimitsPtzField) {
            *limits = m_data.logicalLimits;
            return true;
        } else {
            return false;
        }
    }
}

bool QnCachingPtzController::getFlip(Qt::Orientations *flip) {
    QMutexLocker locker(&m_mutex);
    if(m_data.fields & Qn::FlipPtzField) {
        *flip = m_data.flip;
        return true;
    } else {
        return false;
    }
}

bool QnCachingPtzController::createPreset(const QnPtzPreset &preset) {
	return base_type::createPreset(preset); 
}

bool QnCachingPtzController::updatePreset(const QnPtzPreset &preset) {
	return base_type::updatePreset(preset);
}

bool QnCachingPtzController::removePreset(const QString &presetId) {
	return base_type::removePreset(presetId);
}

bool QnCachingPtzController::activatePreset(const QString &presetId, qreal speed) {
	return base_type::activatePreset(presetId, speed);
}

bool QnCachingPtzController::getPresets(QnPtzPresetList *presets) {
    QMutexLocker locker(&m_mutex);
    if(m_data.fields & Qn::PresetsPtzField) {
        *presets = m_data.presets;
        return true;
    } else {
        return false;
    }
}

bool QnCachingPtzController::createTour(const QnPtzTour &tour) {
	return base_type::createTour(tour); 
}

bool QnCachingPtzController::removeTour(const QString &tourId) {
	return base_type::removeTour(tourId); 
}

bool QnCachingPtzController::activateTour(const QString &tourId) {
	return base_type::activateTour(tourId); 
}

bool QnCachingPtzController::getTours(QnPtzTourList *tours) {
    QMutexLocker locker(&m_mutex);
    if(m_data.fields & Qn::ToursPtzField) {
        *tours = m_data.tours;
        return true;
    } else {
        return false;
    }
}

bool QnCachingPtzController::getData(Qn::PtzDataFields query, QnPtzData *data) {
    QMutexLocker locker(&m_mutex);
	*data = m_data;
	data->query = query;
	data->fields &= query;
	return true;
}

bool QnCachingPtzController::synchronize(Qn::PtzDataFields query) {
	return base_type::synchronize(query);
}

void QnCachingPtzController::updateCacheLocked(const QnPtzData &data) {
	Qn::PtzDataFields fields = data.fields & ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
	if(fields == Qn::NoPtzFields)
		return;

	if(fields & Qn::DeviceLimitsPtzField)	m_data.deviceLimits = data.deviceLimits;
	if(fields & Qn::LogicalLimitsPtzField)	m_data.logicalLimits = data.logicalLimits;
    if(fields & Qn::FlipPtzField)			m_data.flip = data.flip;
    if(fields & Qn::PresetsPtzField)		m_data.presets = data.presets;
    if(fields & Qn::ToursPtzField)			m_data.tours = data.tours;
    m_data.fields |= fields;
}

void QnCachingPtzController::at_baseController_finished(Qn::PtzCommand command, const QVariant &data) {
	if(data.isValid()) {
		QMutexLocker locker(&m_mutex);
		switch (command) {
		case Qn::CreatePresetPtzCommand:
			if(m_data.fields & Qn::PresetsPtzField)
				m_data.presets.append(data.value<QnPtzPreset>());
			break;
		case Qn::UpdatePresetPtzCommand:
			if(m_data.fields & Qn::PresetsPtzField) {
				QnPtzPreset preset = data.value<QnPtzPreset>();
				for(int i = 0; i < m_data.presets.size(); i++) {
					if(m_data.presets[i].id == preset.id) {
						m_data.presets[i] = preset;
						break;
                    }
                }
            }
			break;
		case Qn::RemovePresetPtzCommand:
			if(m_data.fields & Qn::PresetsPtzField) {
                QString presetId = data.value<QString>();
                for(int i = 0; i < m_data.presets.size(); i++) {
                    if(m_data.presets[i].id == presetId) {
                        m_data.presets.removeAt(i);
                        break;
                    }
                }
            }
			break;
		case Qn::CreateTourPtzCommand:
            if(m_data.fields & Qn::ToursPtzField)
                m_data.tours.append(data.value<QnPtzTour>());
            break;
		case Qn::RemoveTourPtzCommand:
            if(m_data.fields & Qn::PresetsPtzField) {
                QString tourId = data.value<QString>();
                for(int i = 0; i < m_data.tours.size(); i++) {
                    if(m_data.tours[i].id == tourId) {
                        m_data.tours.removeAt(i);
                        break;
                    }
                }
            }
            break;
		case Qn::SynchronizePtzCommand:
			updateCacheLocked(data.value<QnPtzData>());
			break;
		default:
			break;
		}
    }

	emit finished(command, data);
}

