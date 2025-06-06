// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qttestoutputreader.h"

#include "qttestresult.h"
#include "qttestframework.h"
#include "../autotesttr.h"
#include "../testtreeitem.h"

#include <qtsupport/qtoutputformatter.h>
#include <utils/qtcassert.h>

#include <QRegularExpression>

using namespace Utils;

namespace Autotest {
namespace Internal {

static QRegularExpression userFileLocation()
{
    static const QRegularExpression regex("^.*\\bfile://((?<file>\\S+))(:(?<line>\\d+))\\b.*$",
                                          QRegularExpression::DotMatchesEverythingOption);
    return regex;
}
static QString decode(const QString& original)
{
    QString result(original);
    static const QRegularExpression regex("&#((x[[:xdigit:]]+)|(\\d+));");

    QRegularExpressionMatchIterator it = regex.globalMatch(original);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString value = match.captured(1);
        if (value.startsWith('x'))
            result.replace(match.captured(0), QChar(value.mid(1).toInt(nullptr, 16)));
        else
            result.replace(match.captured(0), QChar(value.toInt(nullptr, 10)));
    }

    return result;
}

// adapted from qplaintestlogger.cpp
static QString formatResult(double value)
{
    //NAN is not supported with visual studio 2010
    if (value < 0)// || value == NAN)
        return QString("NAN");
    if (value == 0)
        return QString("0");

    int significantDigits = 0;
    qreal divisor = 1;

    while (value / divisor >= 1) {
        divisor *= 10;
        ++significantDigits;
    }

    QString beforeDecimalPoint = QString::number(value, 'f', 0);
    QString afterDecimalPoint = QString::number(value, 'f', 20);
    afterDecimalPoint.remove(0, beforeDecimalPoint.size() + 1);

    const int beforeUse = qMin(beforeDecimalPoint.size(), significantDigits);
    const int beforeRemove = beforeDecimalPoint.size() - beforeUse;

    beforeDecimalPoint.chop(beforeRemove);
    for (int i = 0; i < beforeRemove; ++i)
        beforeDecimalPoint.append('0');

    int afterUse = significantDigits - beforeUse;
    if (beforeDecimalPoint == QString("0") && !afterDecimalPoint.isEmpty()) {
        ++afterUse;
        int i = 0;
        while (i < afterDecimalPoint.size() && afterDecimalPoint.at(i) == '0')
            ++i;
        afterUse += i;
    }

    const int afterRemove = afterDecimalPoint.size() - afterUse;
    afterDecimalPoint.chop(afterRemove);

    QString result = beforeDecimalPoint;
    if (afterUse > 0)
        result.append('.');
    result += afterDecimalPoint;

    return result;
}

static QString constructBenchmarkInformation(const QString &metric, double value, int iterations)
{
    QString metricsText;
    if (metric == "WalltimeMilliseconds")         // default
        metricsText = "msecs";
    else if (metric == "CPUTicks")                // -tickcounter
        metricsText = "CPU ticks";
    else if (metric == "Events")                  // -eventcounter
        metricsText = "events";
    else if (metric == "InstructionReads")        // -callgrind
        metricsText = "instruction reads";
    else if (metric == "CPUCycles")               // -perf
        metricsText = "CPU cycles";
    return Tr::tr("%1 %2 per iteration (total: %3, iterations: %4)")
            .arg(formatResult(value), metricsText, formatResult(value * double(iterations)))
            .arg(iterations);
}

QtTestOutputReader::QtTestOutputReader(Process *testApplication,
                                       const FilePath &buildDirectory,
                                       const FilePath &projectFile,
                                       OutputMode mode, TestType type)
    : TestOutputReader(testApplication, buildDirectory)
    , m_projectFile(projectFile)
    , m_mode(mode)
    , m_testType(type)
{
    m_parseMessages = theQtTestFramework().parseMessages();
}

void QtTestOutputReader::processOutputLine(const QByteArray &outputLine)
{
    static const QByteArray qmlDebug = "QML Debugger: Waiting for connection on port";
    switch (m_mode) {
    case PlainText:
        processPlainTextOutput(outputLine);
        break;
    case XML:
        if (m_xmlReader.tokenType() == QXmlStreamReader::NoToken && outputLine.startsWith(qmlDebug))
            return;
        processXMLOutput(outputLine);
        break;
    }
}

TestResult QtTestOutputReader::createDefaultResult() const
{
    return QtTestResult(id(), m_className, m_projectFile, m_testType, m_testCase, m_dataTag);
}

static QString trQtVersion(const QString &version)
{
    return Tr::tr("Qt version: %1").arg(version);
}

static QString trQtBuild(const QString &build)
{
    return Tr::tr("Qt build: %1").arg(build);
}

static QString trQtestVersion(const QString &test)
{
    return Tr::tr("QTest version: %1").arg(test);
}

void QtTestOutputReader::processXMLOutput(const QByteArray &outputLine)
{
    static QStringList validEndTags = {QStringLiteral("Incident"),
                                       QStringLiteral("Message"),
                                       QStringLiteral("BenchmarkResult"),
                                       QStringLiteral("QtVersion"),
                                       QStringLiteral("QtBuild"),
                                       QStringLiteral("QTestVersion")};

    if (m_className.isEmpty() && outputLine.trimmed().isEmpty())
        return;

    if (m_expectTag) {
        for (auto ch : outputLine) {
            if (std::isspace(ch))
                continue;
            if (ch != '<')
                return;
            break;
        }
    }
    if (m_cdataMode == Description)
        m_xmlReader.addData("\n");
    m_xmlReader.addData(QString::fromUtf8(outputLine));
    while (!m_xmlReader.atEnd()) {
        QXmlStreamReader::TokenType token = m_xmlReader.readNext();
        switch (token) {
        case QXmlStreamReader::StartDocument:
            m_className.clear();
            break;
        case QXmlStreamReader::EndDocument:
            m_xmlReader.clear();
            return;
        case QXmlStreamReader::StartElement: {
            const QString currentTag = m_xmlReader.name().toString();
            if (currentTag == QStringLiteral("TestCase")) {
                m_className = m_xmlReader.attributes().value(QStringLiteral("name")).toString();
                QTC_ASSERT(!m_className.isEmpty(), continue);
                sendStartMessage(false);
            } else if (currentTag == QStringLiteral("TestFunction")) {
                m_testCase = m_xmlReader.attributes().value(QStringLiteral("name")).toString();
                QTC_ASSERT(!m_testCase.isEmpty(), continue);
                if (m_testCase == m_formerTestCase)  // don't report "Executing..." more than once
                    continue;
                sendStartMessage(true);
                sendMessageCurrentTest();
            } else if (currentTag == QStringLiteral("Duration")) {
                m_duration = m_xmlReader.attributes().value(QStringLiteral("msecs")).toString();
                QTC_ASSERT(!m_duration.isEmpty(), continue);
            } else if (currentTag == QStringLiteral("Message")
                       || currentTag == QStringLiteral("Incident")) {
                m_description.clear();
                m_duration.clear();
                m_file.clear();
                m_result = ResultType::Invalid;
                m_lineNumber = 0;
                const QXmlStreamAttributes &attributes = m_xmlReader.attributes();
                m_result = TestResult::resultFromString(
                            attributes.value(QStringLiteral("type")).toString());
                const QString file = decode(attributes.value(QStringLiteral("file")).toString());
                m_file = constructSourceFilePath(m_buildDir, file);
                m_lineNumber = attributes.value(QStringLiteral("line")).toInt();
            } else if (currentTag == QStringLiteral("BenchmarkResult")) {
                const QXmlStreamAttributes &attributes = m_xmlReader.attributes();
                const QString metric = attributes.value(QStringLiteral("metric")).toString();
                const double value = attributes.value(QStringLiteral("value")).toDouble();
                const int iterations = attributes.value(QStringLiteral("iterations")).toInt();
                m_dataTag = attributes.value(QStringLiteral("tag")).toString();
                m_description = constructBenchmarkInformation(metric, value, iterations);
                m_result = ResultType::Benchmark;
            } else if (currentTag == QStringLiteral("DataTag")) {
                m_cdataMode = DataTag;
            } else if (currentTag == QStringLiteral("Description")) {
                m_cdataMode = Description;
            } else if (currentTag == QStringLiteral("QtVersion")) {
                m_result = ResultType::MessageInternal;
                m_cdataMode = QtVersion;
            } else if (currentTag == QStringLiteral("QtBuild")) {
                m_result = ResultType::MessageInternal;
                m_cdataMode = QtBuild;
            } else if (currentTag == QStringLiteral("QTestVersion")) {
                m_result = ResultType::MessageInternal;
                m_cdataMode = QTestVersion;
            }
            break;
        }
        case QXmlStreamReader::Characters: {
            m_expectTag = false;
            const QStringView text = m_xmlReader.text().trimmed();
            if (text.isEmpty())
                break;

            switch (m_cdataMode) {
            case DataTag:
                m_dataTag = text.toString();
                break;
            case Description:
                if (!m_description.isEmpty())
                    m_description.append('\n');
                m_description.append(text.toString());
                break;
            case QtVersion:
                m_description = trQtVersion(text.toString());
                break;
            case QtBuild:
                m_description = trQtBuild(text.toString());
                break;
            case QTestVersion:
                m_description = trQtestVersion(text.toString());
                break;
            default:
                // this must come from plain printf() calls - but this will be ignored anyhow
                qWarning() << "AutoTest.Run: Ignored plain output:" << text.toString();
                break;
            }
            break;
        }
        case QXmlStreamReader::EndElement: {
            m_expectTag = true;
            m_cdataMode = None;
            const QStringView currentTag = m_xmlReader.name();
            if (currentTag == QStringLiteral("TestFunction")) {
                sendFinishMessage(true);
                // TODO: bump progress?
                m_formerTestCase = m_testCase;
                m_testCase.clear();
            } else if (currentTag == QStringLiteral("TestCase")) {
                sendFinishMessage(false);
                m_executionDuration = qRound(m_duration.toDouble());
            } else if (validEndTags.contains(currentTag.toString())) {
                if (m_parseMessages && isTestMessage(m_result)) {
                    const QRegularExpressionMatch match = userFileLocation().match(m_description);
                    if (match.hasMatch()) {
                        // we need to handle possible automatic masking done by qDebug()
                        processLocationOutput(match.captured("file").replace("\\\\", "\\"),
                                              match.captured("line"));
                    }
                }
                sendCompleteInformation();
                if (currentTag == QStringLiteral("Incident"))
                    m_dataTag.clear();
            }
            break;
        }
        default:
            // premature end happens e.g. if not all data has been added to the reader yet
            if (m_xmlReader.error() != QXmlStreamReader::NoError
                    && m_xmlReader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
                createAndReportResult(Tr::tr("XML parsing failed.")
                                      + QString(" (%1) ").arg(m_xmlReader.error())
                                      + m_xmlReader.errorString(), ResultType::MessageFatal);
            }
            break;
        }
    }
}

static QStringList extractFunctionInformation(const QString &testClassName,
                                              const QString &lineWithoutResultType,
                                              ResultType resultType)
{
    static const QRegularExpression classInformation("^(.+?)\\((.*?)\\)(.*)$");
    QStringList result;
    const QRegularExpressionMatch match = classInformation.match(lineWithoutResultType);
    if (match.hasMatch()) {
        QString fullQualifiedFunc = match.captured(1);
        QTC_ASSERT(fullQualifiedFunc.startsWith(testClassName + "::"), return result);
        fullQualifiedFunc = fullQualifiedFunc.mid(testClassName.length() + 2);
        result.append(fullQualifiedFunc);
        if (resultType == ResultType::Benchmark) { // tag is displayed differently
            QString possiblyTag = match.captured(3);
            if (!possiblyTag.isEmpty())
                possiblyTag = possiblyTag.mid(2, possiblyTag.length() - 4);
            result.append(possiblyTag);
            result.append(QString());
        } else {
            result.append(match.captured(2));
            result.append(match.captured(3));
        }
    }
    return result;
}

void QtTestOutputReader::processPlainTextOutput(const QByteArray &outputLine)
{
    static const QRegularExpression start("^.*[*]{9} Start testing of (.*) [*]{9}$");
    static const QRegularExpression config("^Config: Using QtTest library (.*), "
                                           "(Qt (\\d+(\\.\\d+){2}) \\(.*\\))$");
    static const QRegularExpression summary("^Totals: (\\d+) passed, (\\d+) failed, "
                                            "(\\d+) skipped(, (\\d+) blacklisted)?(, (\\d+)ms)?$");
    static const QRegularExpression finish("^[*]{9} Finished testing of (.*) [*]{9}$");

    static const QRegularExpression result("^.*(PASS   |FAIL!  |XFAIL  |XPASS  |SKIP   |RESULT "
                                           "|BPASS  |BFAIL  |BXPASS |BXFAIL "
                                           "|INFO   |QWARN  |WARNING|QDEBUG |QSYSTEM|QCRITICAL): (.*)$");

    static const QRegularExpression benchDetails("^\\s+([\\d,.]+ .* per iteration "
                                                 "\\(total: [\\d,.]+, iterations: \\d+\\))$");
    static const QRegularExpression locationUnix(QT_TEST_FAIL_UNIX_REGEXP);
    static const QRegularExpression locationWin(QT_TEST_FAIL_WIN_REGEXP);

    const QString line = QString::fromUtf8(outputLine);
    QRegularExpressionMatch match;

    auto hasMatch = [&match, line](const QRegularExpression &regex) {
        match = regex.match(line);
        return match.hasMatch();
    };

    if (hasMatch(result)) {
        processResultOutput(match.captured(1).toLower().trimmed(), match.captured(2));
        if (m_parseMessages && isTestMessage(m_result) && hasMatch(userFileLocation())) {
            // we need to handle possible automatic masking done by qDebug()
            processLocationOutput(match.captured("file").replace("\\\\", "\\"),
                                  match.captured("line"));
        }
    } else if (hasMatch(locationUnix)) {
        processLocationOutput(match.captured("file"), match.captured("line"));
    } else if (hasMatch(locationWin)) {
        processLocationOutput(match.captured("file"), match.captured("line"));
    } else if (hasMatch(benchDetails)) {
        m_description = match.captured(1);
    } else if (hasMatch(config)) {
        handleAndSendConfigMessage(match);
    } else if (hasMatch(start)) {
        m_className = match.captured(1);
        QTC_CHECK(!m_className.isEmpty());
        sendStartMessage(false);
    } else if (hasMatch(summary)) {
        m_summary[ResultType::Pass] = match.captured(1).toInt();
        m_summary[ResultType::Fail] = match.captured(2).toInt();
        m_summary[ResultType::Skip] = match.captured(3).toInt();
        // BlacklistedXYZ is wrong here, but we use it for convenience (avoids another enum value)
        if (int blacklisted = match.captured(5).toInt())
            m_summary[ResultType::BlacklistedPass] = blacklisted;
        if (match.hasCaptured(7))
            m_executionDuration.emplace(match.captured(7).toInt());
        processSummaryFinishOutput();
    } else if (finish.match(line).hasMatch()) {
        processSummaryFinishOutput();
    } else { // we have some plain output, but we cannot say where for sure it belongs to..
        if (!m_description.isEmpty())
            m_description.append('\n');
        m_description.append(line);
    }
}

void QtTestOutputReader::processResultOutput(const QString &result, const QString &message)
{
    if (!m_testCase.isEmpty()) { // report the former result if there is any
        sendCompleteInformation();
        m_dataTag.clear();
        m_description.clear();
        m_file.clear();
        m_lineNumber = 0;
    }
    m_result = TestResult::resultFromString(result);
    const QStringList funcWithTag = extractFunctionInformation(m_className, message, m_result);
    QTC_ASSERT(funcWithTag.size() == 3, return);
    m_testCase = funcWithTag.at(0);
    if (m_testCase != m_formerTestCase) { // new test function executed
        if (!m_formerTestCase.isEmpty()) {
            using namespace std;
            swap(m_testCase, m_formerTestCase); // we want formerTestCase to be reported
            sendFinishMessage(true);
            swap(m_testCase, m_formerTestCase);
        }
        sendStartMessage(true);
        sendMessageCurrentTest();
    }
    m_dataTag = funcWithTag.at(1);
    const QString description = funcWithTag.at(2);
    if (!description.isEmpty()) {
        if (!m_description.isEmpty())
            m_description.append('\n');
        m_description.append(description.mid(1)); // cut the first whitespace
    }
    m_formerTestCase = m_testCase;
}

void QtTestOutputReader::processLocationOutput(const QStringView file, const QStringView line)
{
    QTC_ASSERT(!file.isEmpty(), return);
    m_file = constructSourceFilePath(m_buildDir, file.toString());
    m_lineNumber = m_file.isEmpty() ? 0 : line.toInt();
}

void QtTestOutputReader::processSummaryFinishOutput()
{
    if (m_className.isEmpty()) // we have reported already
        return;
    // we still have something to report
    sendCompleteInformation();
    m_dataTag.clear();
    // report finished function
    sendFinishMessage(true);
    m_testCase.clear();
    m_formerTestCase.clear();
    // create and report the finish message for this test class
    sendFinishMessage(false);
    m_className.clear();
    m_description.clear();
    m_result = ResultType::Invalid;
    m_file.clear();
    m_lineNumber = 0;
}

void QtTestOutputReader::sendCompleteInformation()
{
    TestResult testResult = createDefaultResult();
    testResult.setResult(m_result);

    if (m_lineNumber) {
        testResult.setFileName(m_file);
        testResult.setLine(m_lineNumber);
    } else {
        const ITestTreeItem *testItem = testResult.findTestTreeItem();
        if (testItem && testItem->line()) {
            testResult.setFileName(testItem->filePath());
            testResult.setLine(testItem->line());
        }
    }
    testResult.setDescription(m_description);
    if (!m_duration.isEmpty())
        testResult.setDuration(m_duration);
    reportResult(testResult);
}

void QtTestOutputReader::sendMessageCurrentTest()
{
    QtTestResult result({}, {}, m_projectFile, m_testType);
    result.setResult(ResultType::MessageCurrentTest);
    result.setDescription(Tr::tr("Entering test function %1::%2").arg(m_className, m_testCase));
    reportResult(result);
}

void QtTestOutputReader::sendStartMessage(bool isFunction)
{
    TestResult result = createDefaultResult();
    result.setResult(ResultType::TestStart);
    result.setDescription(isFunction ? Tr::tr("Executing test function %1").arg(m_testCase)
                                     : Tr::tr("Executing test case %1").arg(m_className));
    const ITestTreeItem *testItem = result.findTestTreeItem();
    if (testItem && testItem->line()) {
        result.setFileName(testItem->filePath());
        result.setLine(testItem->line());
    }
    reportResult(result);
}

void QtTestOutputReader::sendFinishMessage(bool isFunction)
{
    m_dataTag.clear();
    if (!isFunction)
        m_testCase.clear();
    TestResult result = createDefaultResult();
    result.setResult(ResultType::TestEnd);
    if (!m_duration.isEmpty()) {
        result.setDescription(isFunction ? Tr::tr("Execution took %1 ms.").arg(m_duration)
                                         : Tr::tr("Test execution took %1 ms.").arg(m_duration));
        result.setDuration(m_duration);
    } else {
        result.setDescription(isFunction ? Tr::tr("Test function finished.")
                                         : Tr::tr("Test finished."));
    }
    reportResult(result);
}

void QtTestOutputReader::handleAndSendConfigMessage(const QRegularExpressionMatch &config)
{
    TestResult result = createDefaultResult();
    result.setResult(ResultType::MessageInternal);
    result.setDescription(trQtVersion(config.captured(3)));
    reportResult(result);

    result = createDefaultResult();
    result.setResult(ResultType::MessageInternal);
    result.setDescription(trQtBuild(config.captured(2)));
    reportResult(result);

    result = createDefaultResult();
    result.setResult(ResultType::MessageInternal);
    result.setDescription(trQtestVersion(config.captured(1)));
    reportResult(result);
}

} // namespace Internal
} // namespace Autotest
