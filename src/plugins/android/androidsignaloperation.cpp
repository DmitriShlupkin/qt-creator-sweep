// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "androidsignaloperation.h"

#include "androidconfigurations.h"

#include <utils/qtcprocess.h>

using namespace Tasking;
using namespace Utils;
using namespace std::chrono_literals;

namespace Android::Internal {

AndroidSignalOperation::AndroidSignalOperation() = default;

void AndroidSignalOperation::signalOperationViaADB(qint64 pid, int signal)
{
    struct InternalStorage {
        FilePath adbPath;
        QString runAs = {};
        Result result = Result::Ok;
    };

    const Storage<InternalStorage> storage({AndroidConfig::adbToolPath()});

    const auto onCatSetup = [storage, pid](Process &process) {
        process.setCommand({storage->adbPath, {"shell", "cat", QString("/proc/%1/cmdline").arg(pid)}});
    };
    const auto onCatDone = [storage, pid](const Process &process, DoneWith result) {
        if (result == DoneWith::Success) {
            storage->runAs = process.stdOut();
            if (!storage->runAs.isEmpty())
                return true;
            storage->result = Result::Error("Cannot find User for process: " + QString::number(pid));
        } else if (result == DoneWith::Error) {
            QString result = " adb process exit code: " + QString::number(process.exitCode());
            const QString adbError = process.errorString();
            if (!adbError.isEmpty())
                result += " adb process error: " + adbError;
            storage->result = Result::Error(result);
        } else {
            storage->result = Result::Error("adb process timed out");
        }
        return false;
    };

    const auto onKillSetup = [storage, pid, signal](Process &process) {
        process.setCommand({storage->adbPath, {"shell", "run-as", storage->runAs, "kill",
                                               QString("-%1").arg(signal), QString::number(pid)}});
    };
    const auto onKillDone = [storage, pid](const Process &process, DoneWith result) {
        if (result == DoneWith::Error) {
            storage->result = Result::Error("Cannot kill process: " + QString::number(pid)
                                            + process.stdErr());
        } else if (result == DoneWith::Cancel) {
            storage->result = Result::Error("adb process timed out");
        }
    };

    const auto onDone = [this, storage] { emit finished(storage->result); };

    const Group recipe {
        storage,
        ProcessTask(onCatSetup, onCatDone).withTimeout(5s),
        ProcessTask(onKillSetup, onKillDone).withTimeout(5s),
        onGroupDone(onDone)
    };
    m_taskTreeRunner.start(recipe);
}

void AndroidSignalOperation::killProcess(qint64 pid)
{
    signalOperationViaADB(pid, 9);
}

void AndroidSignalOperation::killProcess(const QString &filePath)
{
    Q_UNUSED(filePath)
    emit finished(Result::Error("The android signal operation does not support killing by filepath."));
}

void AndroidSignalOperation::interruptProcess(qint64 pid)
{
    signalOperationViaADB(pid, 2);
}

} // namespace Android::Internal
