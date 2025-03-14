// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "debugger_global.h"
#include "debuggerconstants.h"
#include "debuggerengine.h"

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/runcontrol.h>

namespace Debugger {

namespace Internal { class DebuggerRunToolPrivate; }

class DEBUGGER_EXPORT DebuggerRunTool final : public ProjectExplorer::RunWorker
{
    Q_OBJECT

public:
    explicit DebuggerRunTool(ProjectExplorer::RunControl *runControl);
    ~DebuggerRunTool() override;

    void start() final;
    void stop() final;

    void setupPortsGatherer();

    DebuggerRunParameters &runParameters();

signals:
    void canceled();

private:
    Internal::DebuggerRunToolPrivate *d;
};

void setupDebuggerRunWorker();

class SimpleDebugRunnerFactory final : public ProjectExplorer::RunWorkerFactory
{
public:
    explicit SimpleDebugRunnerFactory(const QList<Utils::Id> &runConfigs, const QList<Utils::Id> &extraRunModes = {})
    {
        cloneProduct(Constants::DEBUGGER_RUN_FACTORY);
        addSupportedRunMode(ProjectExplorer::Constants::DEBUG_RUN_MODE);
        for (const Utils::Id &id : extraRunModes)
            addSupportedRunMode(id);
        setSupportedRunConfigs(runConfigs);
    }
};

extern DEBUGGER_EXPORT const char DebugServerRunnerWorkerId[];
extern DEBUGGER_EXPORT const char GdbServerPortGathererWorkerId[];

} // Debugger
