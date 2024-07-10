// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmljseditor_global.h"

#include <QtCore/qversionnumber.h>
#include <utils/filepath.h>
#include <QMutex>
#include <QObject>

namespace QmlJSEditor {

struct QmllsSettings
{
    static const inline QVersionNumber mininumQmllsVersion = QVersionNumber(6,8);
    bool useQmlls = true;
    bool useLatestQmlls = false;
    bool ignoreMinimumQmllsVersion = false;
    bool disableBuiltinCodemodel = false;
    bool generateQmllsIniFiles = false;

    friend bool operator==(const QmllsSettings &s1, const QmllsSettings &s2)
    {
        return s1.useQmlls == s2.useQmlls && s1.useLatestQmlls == s2.useLatestQmlls
               && s1.disableBuiltinCodemodel == s2.disableBuiltinCodemodel
               && s1.generateQmllsIniFiles == s2.generateQmllsIniFiles
               && s1.ignoreMinimumQmllsVersion == s2.ignoreMinimumQmllsVersion;
    }
    friend bool operator!=(const QmllsSettings &s1, const QmllsSettings &s2) { return !(s1 == s2); }
};

class QMLJSEDITOR_EXPORT QmllsSettingsManager : public QObject
{
    Q_OBJECT
public:
    static QmllsSettingsManager *instance();
    QmllsSettings lastSettings();
    Utils::FilePath latestQmlls();
    void setupAutoupdate();
public slots:
    void checkForChanges();
signals:
    void settingsChanged();

private:
    QMutex m_mutex;
    QmllsSettings m_lastSettings;
    Utils::FilePath m_latestQmlls;
};

} // namespace QmlJSEditor
