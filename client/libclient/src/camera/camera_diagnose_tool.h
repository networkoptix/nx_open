#pragma once

#include <QObject>

#include <client_core/connection_context_aware.h>

#include <api/model/camera_diagnostics_reply.h>
#include <core/resource/resource_fwd.h>
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
    class DiagnoseTool
    :
        public QObject,
        public QnConnectionContextAware
    {
        Q_OBJECT

    public:
        // TODO: #Elric #enum
        enum State
        {
            //!Diagnostics has not been started yet
            sInit,
            //!Diagnostics running
            sInProgress,
            //!Done
            sDone
        };

        DiagnoseTool( const QnUuid& cameraId, QObject *parent = nullptr );
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

    public slots:
        void onGetServerSystemIdResponse( int status, QString serverSystemId, int handle );
        //!Receives response from server
        /*!
            \param status \a QNetworkReply::NetworkError
            \param performedStep Constant from \a Step::Value enumeration
        */
        void onCameraDiagnosticsStepResponse( int status, QnCameraDiagnosticsReply, int handle );

    signals:
        /*!
            \param stepType Value from CameraDiagnostics::Step::Value enumeration
        */
        void diagnosticsStepStarted( CameraDiagnostics::Step::Value stepType );
        /*!
            \param stepType Value from CameraDiagnostics::Step::Value enumeration
        */
        void diagnosticsStepResult( CameraDiagnostics::Step::Value stepType, bool result, const QString &errorMessage );
        //!Emmitted on diagnostics done (with any result)
        /*!
            \param finalStep Value from CameraDiagnostics::Step::Value enumeration
            \param result \a true, if diagnostics found no errors
            \param finalStep Step, diagnostics has been stopped on
        */
        void diagnosticsDone( CameraDiagnostics::Step::Value finalStep, bool result, const QString &errorMessage );

    private:
        const QnUuid m_cameraId;
        State m_state;
        Step::Value m_step;
        QString m_serverHostAddress;
        QnMediaServerResourcePtr m_server;
        QnVirtualCameraResourcePtr m_camera;
        bool m_result;
        QString m_errorMessage;

        void doMediaServerAvailabilityStep();
    };
}
