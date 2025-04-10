// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "remotelinuxdebugsupport.h"

#include "remotelinux_constants.h"

#include <debugger/debuggerruncontrol.h>

#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/qmldebugcommandlinearguments.h>
#include <projectexplorer/runconfigurationaspects.h>

#include <utils/qtcprocess.h>

using namespace Debugger;
using namespace ProjectExplorer;
using namespace Utils;

namespace RemoteLinux::Internal {

static const QList<Id> supportedRunConfigs()
{
    return {
        Constants::RunConfigId,
        Constants::CustomRunConfigId,
        "QmlProjectManager.QmlRunConfiguration"
    };
}

class RemoteLinuxRunWorkerFactory final : public RunWorkerFactory
{
public:
    RemoteLinuxRunWorkerFactory()
    {
        setRecipeProducer([](RunControl *runControl) { return processRecipe(runControl); });
        addSupportedRunMode(ProjectExplorer::Constants::NORMAL_RUN_MODE);
        addSupportedDeviceType(Constants::GenericLinuxOsType);
        setSupportedRunConfigs(supportedRunConfigs());
    }
};

class RemoteLinuxDebugWorkerFactory final : public ProjectExplorer::RunWorkerFactory
{
public:
    RemoteLinuxDebugWorkerFactory()
    {
        setProducer([](RunControl *rc) {
            rc->requestDebugChannel();

            auto debugger = new DebuggerRunTool(rc);
            DebuggerRunParameters &rp = debugger->runParameters();
            debugger->setId("RemoteLinuxDebugWorker");
            debugger->setupPortsGatherer();
            rp.setUseTerminal(false);
            rp.setAddQmlServerInferiorCmdArgIfNeeded(true);

            rp.setStartMode(AttachToRemoteServer);
            rp.setCloseMode(KillAndExitMonitorAtClose);
            rp.setUseExtendedRemote(true);

            if (rc->device()->osType() == Utils::OsTypeMac)
                rp.setLldbPlatform("remote-macosx");
            else
                rp.setLldbPlatform("remote-linux");
            return debugger;
        });
        addSupportedRunMode(ProjectExplorer::Constants::DEBUG_RUN_MODE);
        addSupportedDeviceType(Constants::GenericLinuxOsType);
        setSupportedRunConfigs(supportedRunConfigs());
    }
};

class RemoteLinuxQmlToolingWorkerFactory final : public ProjectExplorer::RunWorkerFactory
{
public:
    RemoteLinuxQmlToolingWorkerFactory()
    {
        setProducer([](RunControl *runControl) {
            runControl->requestQmlChannel();

            auto runworker = runControl->createWorker(runnerIdForRunMode(runControl->runMode()));

            const auto modifier = [runControl](Process &process) {
                QmlDebugServicesPreset services = servicesForRunMode(runControl->runMode());

                CommandLine cmd = runControl->commandLine();
                cmd.addArg(qmlDebugTcpArguments(services, runControl->qmlChannel()));
                process.setCommand(cmd);
            };
            auto worker = createProcessWorker(runControl, modifier);
            runworker->addStartDependency(worker);
            worker->addStopDependency(runworker);
            return worker;
        });
        addSupportedRunMode(ProjectExplorer::Constants::QML_PROFILER_RUN_MODE);
        addSupportedRunMode(ProjectExplorer::Constants::QML_PREVIEW_RUN_MODE);
        addSupportedDeviceType(Constants::GenericLinuxOsType);
        setSupportedRunConfigs(supportedRunConfigs());
    }
};

void setupRemoteLinuxRunAndDebugSupport()
{
    static RemoteLinuxRunWorkerFactory runWorkerFactory;
    static RemoteLinuxDebugWorkerFactory debugWorkerFactory;
    static RemoteLinuxQmlToolingWorkerFactory qmlToolingWorkerFactory;
}

} // RemoteLinux::Internal
