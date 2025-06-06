// Copyright (C) 2019 Klarälvdalens Datakonsult AB, a KDAB Group company,
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "ctftimelinemodel.h"

#include "ctftracemanager.h"
#include "ctfvisualizerconstants.h"
#include "ctfvisualizertr.h"

#include <tracing/timelineformattime.h>

#include <utils/qtcassert.h>

#include <string>

namespace CtfVisualizer::Internal {

using json = nlohmann::json;
using namespace Constants;

CtfTimelineModel::CtfTimelineModel(Timeline::TimelineModelAggregator *parent,
                                   CtfTraceManager *traceManager,
                                   const QString &tid,
                                   const QString &pid)
    : Timeline::TimelineModel(parent)
    , m_traceManager(traceManager)
    , m_threadId(tid)
    , m_processId(pid)
{
    updateName();
    setCollapsedRowCount(1);
    setCategoryColor(colorByHue(int(qHash(pid))));
    setHasMixedTypesInExpandedState(true);
}

QRgb CtfTimelineModel::color(int index) const
{
    return colorBySelectionId(index);
}

QVariantList CtfTimelineModel::labels() const
{
    QVariantList result;

    QList<std::string> sortedCounterNames = m_counterNames;
    std::sort(sortedCounterNames.begin(), sortedCounterNames.end());
    for (int i = 0; i < sortedCounterNames.size(); ++i) {
        QVariantMap element;
        element.insert(QLatin1String("description"), QString("~ %1").arg(QString::fromStdString(sortedCounterNames[i])));
        element.insert(QLatin1String("id"), i);
        result << element;
    }

    for (int i = 0; i < m_maxStackSize; ++i) {
        QVariantMap element;
        element.insert(QLatin1String("description"),
                       QStringLiteral("- ").append(Tr::tr("Stack Level %1").arg(i)));
        element.insert(QLatin1String("id"), m_counterNames.size() + i);
        result << element;
    }
    return result;
}

QVariantMap CtfTimelineModel::orderedDetails(int index) const
{
    QMap<int, QPair<QString, QString>> info = m_details.value(index);
    const int counterIdx = m_itemToCounterIdx.value(index, 0);
    if (counterIdx > 0) {
        // this item is a counter, add its properties:
        info.insert(0, {{}, QString::fromStdString(m_counterNames.at(counterIdx - 1))});
        info.insert(4, {Tr::tr("Value"),
                        QString::number(double(m_counterValues.at(index)), 'g')});
        info.insert(5, {Tr::tr("Min"),
                        QString::number(double(m_counterData.at(counterIdx - 1).min), 'g')});
        info.insert(6, {Tr::tr("Max"),
                        QString::number(double(m_counterData.at(counterIdx - 1).max), 'g')});
    }
    info.insert(2, {Tr::tr("Start"), Timeline::formatTime(startTime(index))});
    info.insert(3, {Tr::tr("Wall Duration"), Timeline::formatTime(duration(index))});
    QVariantMap data;
    QString title = info.value(0).second;
    data.insert("title", title);
    QVariantList content;
    auto it = info.constBegin();
    auto end = info.constEnd();
    ++it;  // skip title
    while (it != end) {
        content.append(it.value().first);
        content.append(it.value().second);
        ++it;
    }
    data.insert("content", content);
    emit detailsRequested(title);
    return data;
}

int CtfTimelineModel::expandedRow(int index) const
{
    // m_itemToCounterIdx[index] is 0 for non-counter indexes
    const int counterIdx = m_itemToCounterIdx.value(index, 0);
    if (counterIdx > 0) {
        return m_counterIndexToRow[counterIdx - 1] + 1;
    }
    return m_rows.value(index) + m_counterData.size() + 1;
}

int CtfTimelineModel::collapsedRow(int index) const
{
    Q_UNUSED(index)
    return 0;
}

int CtfTimelineModel::typeId(int index) const
{
    QTC_ASSERT(index >= 0 && index < count(), return -1);
    return selectionId(index);
}

bool CtfTimelineModel::handlesTypeId(int typeId) const
{
    return m_handledTypeIds.contains(typeId);
}

float CtfTimelineModel::relativeHeight(int index) const
{
    // m_itemToCounterIdx[index] is 0 for non-counter indexes
    const int counterIdx = m_itemToCounterIdx.value(index, 0);
    if (counterIdx > 0) {
        const CounterData &data = m_counterData[counterIdx - 1];
        const float value = m_counterValues[index] / std::max(data.max, 1.0f);
        return value;
    }
    return 1.0f;
}

QPair<bool, qint64> CtfTimelineModel::addEvent(const json &event, double timeOffset)
{
    const double timestamp = event.value(CtfTracingClockTimestampKey, 0.0);
    const qint64 normalizedTime = qint64((timestamp - timeOffset) * 1000);
    const std::string eventPhase = event.value(CtfEventPhaseKey, "");
    const std::string name = event.value(CtfEventNameKey, "");
    qint64 duration = -1;

    const int selectionId = m_traceManager->getSelectionId(name);
    m_handledTypeIds.insert(selectionId);

    bool visibleOnTimeline = false;
    if (eventPhase == CtfEventTypeBegin || eventPhase == CtfEventTypeComplete
            || eventPhase == CtfEventTypeInstant || eventPhase == CtfEventTypeInstantDeprecated) {
        duration = newStackEvent(event, normalizedTime, eventPhase, name, selectionId);
        visibleOnTimeline = true;
    } else if (eventPhase == CtfEventTypeEnd) {
        duration = closeStackEvent(event, timestamp, normalizedTime);
        visibleOnTimeline = true;
    } else if (eventPhase == CtfEventTypeCounter) {
        addCounterValue(event, normalizedTime, name, selectionId);
        visibleOnTimeline = true;
    } else if (eventPhase == CtfEventTypeMetadata) {
        const std::string name = event[CtfEventNameKey];
        if (name == "thread_name") {
            m_threadName = QString::fromStdString(event["args"]["name"]);
            updateName();
        } else if (name == "process_name") {
            m_processName = QString::fromStdString(event["args"]["name"]);
            updateName();
        }
    }
    return {visibleOnTimeline, duration};
}

void CtfTimelineModel::finalize(double traceBegin, double traceEnd, const QString &processName, const QString &threadName)
{
    m_processName = processName;
    m_threadName = threadName;
    updateName();

    qint64 normalizedEnd = qint64((traceEnd - traceBegin) * 1000);
    while (!m_openEventIds.isEmpty()) {
        int index = m_openEventIds.pop();
        qint64 duration = normalizedEnd - startTime(index);
        insertEnd(index, duration);
        m_details[index].insert(3, {reuse(Tr::tr("Wall Duration")), Timeline::formatTime(duration)});
        m_details[index].insert(6, {reuse(Tr::tr("Unfinished")), reuse(Tr::tr("true"))});
    }
    computeNesting();
    m_rows = computeRows(&m_maxStackSize);
    ++m_maxStackSize; // index -> count

    QList<std::string> sortedCounterNames = m_counterNames;
    std::sort(sortedCounterNames.begin(), sortedCounterNames.end());
    m_counterIndexToRow.resize(m_counterNames.size());
    for (int i = 0; i < m_counterIndexToRow.size(); ++i) {
        m_counterIndexToRow[i] = sortedCounterNames.indexOf(m_counterNames[i]);
    }

    // we insert an empty row in expanded state because otherwise
    // the row label would be where the thread label is
    setExpandedRowCount(1 + m_counterData.size() + m_maxStackSize);
    emit contentChanged();
}

QString CtfTimelineModel::tid() const
{
    return m_threadId;
}

QString CtfTimelineModel::eventTitle(int index) const
{
    const int counterIdx = m_itemToCounterIdx.value(index, 0);
    if (counterIdx > 0) {
        return QString::fromStdString(m_counterNames.at(counterIdx - 1));
    }
    return m_details.value(index).value(0).second;
}

void CtfTimelineModel::updateName()
{
    if (m_threadName.isEmpty()) {
        setDisplayName(Tr::tr("Thread %1").arg(m_threadId));
    } else {
        setDisplayName(QString("%1 (%2)").arg(m_threadName, m_threadId));
    }
    QString process = m_processName.isEmpty() ? m_processId
                                              : QString("%1 (%2)").arg(m_processName, m_processId);
    QString thread = m_threadName.isEmpty() ? m_threadId
                                            : QString("%1 (%2)").arg(m_threadName, m_threadId);
    setTooltip(QString("Process: %1\nThread: %2").arg(process, thread));
}

qint64 CtfTimelineModel::newStackEvent(const json &event, qint64 normalizedTime,
                                       const std::string &eventPhase, const std::string &name,
                                       int selectionId)
{
    int index = 0;
    qint64 duration = -1;
    if (eventPhase == CtfEventTypeBegin) {
        index = insertStart(normalizedTime, selectionId);
        m_openEventIds.push(index);
        // if this event was inserted before existing events, we need to
        // check whether indexes stored in m_openEventIds need to be updated (increased):
        // (the top element is the current event and is skipped)
        for (int i = m_openEventIds.size() - 2; i >= 0; --i) {
            if (m_openEventIds[i] >= index) ++m_openEventIds[i];
        }
    } else if (eventPhase == CtfEventTypeComplete) {
        duration = qint64(event[CtfDurationKey]) * 1000;
        index = insert(normalizedTime, duration, selectionId);
        for (int i = m_openEventIds.size() - 1; i >= 0; --i) {
            if (m_openEventIds[i] >= index)
                ++m_openEventIds[i];
        }
    } else {
        index = insert(normalizedTime, 0, selectionId);
        for (int i = m_openEventIds.size() - 1; i >= 0; --i) {
            if (m_openEventIds[i] >= index)
                ++m_openEventIds[i];
        }
    }
    if (index >= m_details.size()) {
        m_details.resize(index + 1);
        m_details[index] = QMap<int, QPair<QString, QString>>();
    } else {
        m_details.insert(index, QMap<int, QPair<QString, QString>>());
    }
    if (m_counterValues.size() > index) {
        // if the event was inserted before any counter, we need
        // to shift the values after it and update the last index:
        m_counterValues.insert(index, 0);
        m_itemToCounterIdx.insert(index, 0);
        for (CounterData &counterData: m_counterData) {
            if (counterData.startEventIndex >= index) ++counterData.startEventIndex;
        }
    }

    if (!name.empty()) {
        m_details[index].insert(0, {{}, reuse(QString::fromStdString(name))});
    }
    if (event.contains(CtfEventCategoryKey)) {
        m_details[index].insert(1, {reuse(Tr::tr("Categories")),
                                    reuse(QString::fromStdString(event[CtfEventCategoryKey]))});
    }
    if (event.contains("args") && !event["args"].empty()) {
        QString argsJson = QString::fromStdString(event["args"].dump(1));
        // strip leading and trailing curled brackets:
        argsJson = argsJson.size() > 4 ? argsJson.mid(2, argsJson.size() - 4) : argsJson;
        m_details[index].insert(4, {reuse(Tr::tr("Arguments")), reuse(argsJson)});
    }
    if (eventPhase == CtfEventTypeInstant) {
        m_details[index].insert(6, {reuse(Tr::tr("Instant")), reuse(Tr::tr("true"))});
        if (event.contains("s")) {
            std::string scope = event["s"];
            if (scope == "g") {
                m_details[index].insert(7, {reuse(Tr::tr("Scope")), reuse(Tr::tr("global"))});
            } else if (scope == "p") {
                m_details[index].insert(7, {reuse(Tr::tr("Scope")), reuse(Tr::tr("process"))});
            } else {
                m_details[index].insert(7, {reuse(Tr::tr("Scope")), reuse(Tr::tr("thread"))});
            }
        }
    }
    return duration;
}

qint64 CtfTimelineModel::closeStackEvent(const json &event, double timestamp, qint64 normalizedTime)
{
    if (m_openEventIds.isEmpty()) {
        qWarning() << QString("End event without open 'begin' event at timestamp %1").arg(timestamp, 0, 'f');
        return -1;
    } else {
        const int index = m_openEventIds.pop();
        const qint64 duration = normalizedTime - startTime(index);
        insertEnd(index, duration);
        if (event.contains("args") && !event["args"].empty()) {
            QString argsJson = QString::fromStdString(event["args"].dump(1));
            // strip leading and trailing curled brackets:
            argsJson = argsJson.size() > 4 ? argsJson.mid(2, argsJson.size() - 4) : argsJson;
            m_details[index].insert(5, {reuse(Tr::tr("Return Arguments")), reuse(argsJson)});
        }
        return duration;
    }
}

void CtfTimelineModel::addCounterValue(const json &event, qint64 normalizedTime,
                                       const std::string &name, int selectionId)
{
    if (!event.contains("args")) return;
    // CTF documentation says all keys of 'args' should be displayed in
    // one stacked graph, but we will display them separately:
    for (const auto &it : event["args"].items()) {
        std::string counterName = event.contains("id") ?
                    name + event.value("id", "") : name;
        const std::string &key = it.key();
        if (key != "value") {
            counterName += " " + key;
        }
        const float value = it.value();
        int counterIndex = m_counterNames.indexOf(counterName);
        if (counterIndex < 0) {
            counterIndex = m_counterData.size();
            m_counterNames.append(counterName);
            m_counterData.append(CounterData());
        }
        CounterData &data = m_counterData[counterIndex];
        if (data.startEventIndex >= 0) {
            insertEnd(data.startEventIndex, normalizedTime - data.end);
        }
        const int index = insertStart(normalizedTime, selectionId);
        data.startEventIndex = index;
        data.end = normalizedTime;
        data.min = std::min(data.min, value);
        data.max = std::max(data.max, value);
        if (index >= m_counterValues.size()) {
            m_counterValues.resize(index + 1);
            m_counterValues[index] = value;
            m_itemToCounterIdx.resize(index + 1);
            // m_itemToCounterIdx[index] == 0 is used to indicate a non-counter value
            m_itemToCounterIdx[index] = counterIndex + 1;
        } else {
            m_counterValues.insert(index, value);
            m_itemToCounterIdx.insert(index, counterIndex + 1);
        }
    }
}

const QString &CtfTimelineModel::reuse(const QString &value)
{
    auto it = m_reusableStrings.find(value);
    if (it == m_reusableStrings.end()) {
        m_reusableStrings.insert(value);
        return value;
    }
    return *it;
}

} // namespace CtfVisualizer::Internal

