// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <api/model/camera_diagnostics_reply.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <utils/camera/camera_diagnostics.h>
#include <utils/common/id.h>

namespace CameraDiagnostics
{
    //!Initiates specified camera diagnostics on mediaserver and reports progress
    /*!
        One \a DiagnoseTool instance performs one diagnostics for one camera.

        Diagnostics stops with success if media stream can be successfully received from camera

        Signals are emmitted in following order:\n
        - diagnosticsStarted
        - diagnosticsStepStarted
        - diagnosticsStepResult
        ...
        - diagnosticsStepStarted
        - diagnosticsStepResult
        - diagnosticsDone

        \note Diagnose stops on first error
        \note Diagnostics is done in separate thread
        \note Class methods are reenterable
    */
    class DiagnoseTool:
        public QObject,
        public nx::vms::client::desktop::SystemContextAware
    {
        Q_OBJECT

    public:
        enum State
        {
            //!Diagnostics has not been started yet
            sInit,
            //!Diagnostics running
            sInProgress,
            //!Done
            sDone
        };

        DiagnoseTool(const QnVirtualCameraResourcePtr& camera, QObject* parent = nullptr);
        virtual ~DiagnoseTool();

        //!Starts diagnostics and returns immediately
        void start();
        //!Returns object's current state
        State state() const;
        //!Valid only after \a DiagnoseTool::start call. After diagnostics is finished returns final step
        Step::Value currentStep() const;
        //!Valid only after diagnostics finished
        bool result() const;
        //!Valid only after diagnostics finished
        QString errorMessage() const;

    protected:
        void onGetServerSystemIdResponse(bool success, int handle, QString serverSystemId);
        //!Receives response from server
        /*!
            \param status \a QNetworkReply::NetworkError
            \param performedStep Constant from \a Step::Value enumeration
        */
        void onCameraDiagnosticsStepResponse(
            bool success, int handle, const QnCameraDiagnosticsReply& reply);

        void doNextStep(Step::Value nextStep);

    signals:
        /*!
            \param stepType Value from CameraDiagnostics::Step::Value enumeration
        */
        void diagnosticsStepStarted(int stepType );
        /*!
            \param stepType Value from CameraDiagnostics::Step::Value enumeration
        */
        void diagnosticsStepResult(int stepType, bool result, const QString &errorMessage );
        //!Emmitted on diagnostics done (with any result)
        /*!
            \param finalStep Value from CameraDiagnostics::Step::Value enumeration
            \param result \a true, if diagnostics found no errors
            \param finalStep Step, diagnostics has been stopped on
        */
        void diagnosticsDone(int finalStep, bool result, const QString &errorMessage );

    private:
        QnVirtualCameraResourcePtr m_camera;

        State m_state;
        Step::Value m_step;

        bool m_result;
        QString m_errorMessage;

        void doMediaServerAvailabilityStep();
    };
}
