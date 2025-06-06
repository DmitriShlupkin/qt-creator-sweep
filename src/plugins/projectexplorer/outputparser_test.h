// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#ifdef WITH_TESTS

#include "projectexplorer_export.h"

#include "task.h"

#include <utils/outputformatter.h>

namespace ProjectExplorer {

class TestTerminator;

class PROJECTEXPLORER_EXPORT OutputParserTester : public Utils::OutputFormatter
{
    Q_OBJECT

public:
    enum Channel {
        STDOUT,
        STDERR
    };

    OutputParserTester();
    ~OutputParserTester();

    // test functions:
    void testParsing(const QString &input, Channel inputChannel,
                     Tasks tasks,
                     const QStringList &childStdOutLines,
                     const QStringList &childStdErrLines);

    void setDebugEnabled(bool);

signals:
    void aboutToDeleteParser();

private:
    void reset();

    bool m_debug = false;

    QStringList m_receivedStdErrChildLines;
    QStringList m_receivedStdOutChildLines;
    Tasks m_receivedTasks;

    friend class TestTerminator;
};

QObject *createOutputParserTest();

} // namespace ProjectExplorer

Q_DECLARE_METATYPE(ProjectExplorer::OutputParserTester::Channel)

#endif
