// Copyright (C) 2020 Alexis Jeandet.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/ioutputparser.h>

#include <utils/outputformatter.h>

#include <QRegularExpression>

namespace MesonProjectManager::Internal {

class MesonOutputParser final : public ProjectExplorer::OutputTaskParser
{
    const QRegularExpression m_errorFileLocRegex{R"((^.*meson.build):(\d+):(\d+): ERROR)"};
    const QRegularExpression m_errorOptionRegex{R"!(ERROR: Value)!"};
    int m_remainingLines = 0;
    QStringList m_pending;
    void pushLine(const QString &line);
    Result processErrors(const QString &line);
    Result processWarnings(const QString &line);
    void addTask(ProjectExplorer::Task::TaskType type, const QString &line);
    void addTask(ProjectExplorer::Task task);
    Utils::OutputLineParser::LinkSpecs addTask(ProjectExplorer::Task::TaskType type,
                                               const QString &line,
                                               const QRegularExpressionMatch &match,
                                               int fileCapIndex,
                                               int lineNumberCapIndex);

public:
    MesonOutputParser();
    Result handleLine(const QString &line, Utils::OutputFormat type) override;
    void readStdo(const QByteArray &data);
    void setSourceDirectory(const Utils::FilePath &sourceDir);
    void setBuildDirectory(const Utils::FilePath &buildDir);
};

} // namespace MesonProjectManager::Internal
