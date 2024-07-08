// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "uniquename.h"

#include <model/modelutils.h>

#include <utils/span.h>

#include <QFileInfo>
#include <QRegularExpression>

namespace QmlDesigner::UniqueName {

using namespace Qt::Literals;

namespace {

QString toCamelCase(const QString &input)
{
    QString result = input.at(0).toLower();
    bool capitalizeNext = false;

    for (const QChar &c : Utils::span{input}.subspan(1)) {
        bool isValidChar = c.isLetterOrNumber() || c == '_';
        if (isValidChar)
            result += capitalizeNext ? c.toUpper() : c;

        capitalizeNext = !isValidChar;
    }

    return result;
}

} // namespace

/**
 * @brief Generates a unique name based on the provided name.
 *
 * This method iteratively generates a name by appending suffixes until a unique name is found.
 * The uniqueness of the generated name is determined by the provided predicate function.
 *
 * @param name The original name to be made unique.
 * @param predicate A function that checks if a name exists. Returns true if the name exists,
 *        false if name is unique.
 * @return A unique name derived from the provided name.
 */
QString generate(const QString &name, std::function<bool(const QString &)> predicate)
{
    if (!predicate(name))
        return name;

    // match prefix and number (including zero padding) parts
    static QRegularExpression rgx("(\\D*?)(\\d+)$");
    QRegularExpressionMatch match = rgx.match(name);

    QString prefix;
    int number = 0;
    int padding = 0;

    if (match.hasMatch()) {
        // Split the name into prefix and number
        prefix = match.captured(1);
        QString numberStr = match.captured(2);
        number = numberStr.toInt();
        padding = numberStr.size();
    } else {
        prefix = name;
    }

    QString nameTemplate = "%1%2";
    QString newName;
    do {
        newName = nameTemplate.arg(prefix).arg(++number, padding, 10, QChar('0'));
    } while (predicate(newName));

    return newName;
}

/**
 * @brief Generates a unique path based on the provided path. If the path belongs to a file, the
 * filename or if it's a directory, the directory name will be adjusted to ensure uniqueness.
 *
 * This method appends a numerical suffix (or increment it if it exists) to the filename or
 * directory name if necessary to make it unique.
 *
 * @param path The original path to be made unique.
 * @return A unique path derived from the provided path.
 */
QString generatePath(const QString &path)
{
    // Remove the trailing slash if it exists (otherwise QFileInfo::path() returns empty)
    QString adjustedPath = path;
    if (adjustedPath.endsWith('/'))
        adjustedPath.chop(1);

    QFileInfo fileInfo = QFileInfo(adjustedPath);
    QString baseName = fileInfo.baseName();
    QString suffix = fileInfo.completeSuffix();
    if (!suffix.isEmpty())
        suffix.prepend('.');

    QString parentDir = fileInfo.path();
    QString pathTemplate = parentDir + "/%1" + suffix;

    QString uniqueBaseName = UniqueName::generate(baseName, [&] (const QString &currName) {
        return QFileInfo::exists(pathTemplate.arg(currName));
    });

    return pathTemplate.arg(uniqueBaseName);
}

/**
 * @brief Generates a unique ID based on the provided id
 *
 *  This works similar to get() with additional restrictions:
 *  - Removes non-Latin1 characters
 *  - Removes spaces
 *  - Ensures the first letter is lowercase
 *  - Converts spaces to camel case
 *  - Prepends an underscore if id starts with a number or is a reserved word
 *
 * @param id The original id to be made unique.
 * @return A unique Id (when predicate() returns false)
 */
QString generateId(const QString &id, std::function<bool(const QString &)> predicate)
{
    if (id.isEmpty())
        return {};

    // remove non word (non A-Z, a-z, 0-9) or space characters
    QString newId = id.trimmed();

    newId = toCamelCase(newId);

    // prepend _ if starts with a digit or invalid id (such as reserved words)
    if (newId.at(0).isDigit() || ModelUtils::isBannedQmlId(newId))
        newId.prepend('_');

    if (!predicate)
        return newId;

    return UniqueName::generate(newId, predicate);
}

} // namespace QmlDesigner::UniqueName
