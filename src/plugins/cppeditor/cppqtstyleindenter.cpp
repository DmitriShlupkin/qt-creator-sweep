// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppqtstyleindenter.h"

#include "cppcodeformatter.h"
#include "cpptoolssettings.h"
#include "cppcodestylesettings.h"

#include <QChar>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>

using namespace TextEditor;

namespace CppEditor {
namespace Internal {

class CppQtStyleIndenter final : public TextIndenter
{
public:
    explicit CppQtStyleIndenter(QTextDocument *doc)
        : TextIndenter(doc)
    {
        // Just for safety. setCodeStylePreferences should be called when the editor the
        // indenter belongs to gets initialized.
        m_cppCodeStylePreferences = CppToolsSettings::cppCodeStyle();
    }

    bool isElectricCharacter(const QChar &ch) const final;
    void indentBlock(const QTextBlock &block,
                     const QChar &typedChar,
                     const TabSettings &tabSettings,
                     int cursorPositionInEditor = -1) final;

    void indent(const QTextCursor &cursor,
                const QChar &typedChar,
                const TabSettings &tabSettings,
                int cursorPositionInEditor = -1) final;

    void setCodeStylePreferences(ICodeStylePreferences *preferences) final;
    void invalidateCache() final;
    int indentFor(const QTextBlock &block,
                  const TabSettings &tabSettings,
                  int cursorPositionInEditor = -1) final;
    int visualIndentFor(const QTextBlock &block, const TabSettings &tabSettings) final;
    IndentationForBlock indentationForBlocks(const QList<QTextBlock> &blocks,
                                             const TabSettings &tabSettings,
                                             int cursorPositionInEditor = -1) final;

private:
    CppCodeStyleSettings codeStyleSettings() const;
    CppCodeStylePreferences *m_cppCodeStylePreferences = nullptr;
};

bool CppQtStyleIndenter::isElectricCharacter(const QChar &ch) const
{
    switch (ch.toLatin1()) {
    case '{':
    case '}':
    case ':':
    case '#':
    case '<':
    case '>':
    case ';':
        return true;
    }
    return false;
}

static bool isElectricInLine(const QChar ch, const QString &text)
{
    switch (ch.toLatin1()) {
    case ';':
        return text.contains(QLatin1String("break"));
    case ':':
        // switch cases and access declarations should be reindented
        if (text.contains(QLatin1String("case")) || text.contains(QLatin1String("default"))
            || text.contains(QLatin1String("public")) || text.contains(QLatin1String("private"))
            || text.contains(QLatin1String("protected")) || text.contains(QLatin1String("signals"))
            || text.contains(QLatin1String("Q_SIGNALS"))) {
            return true;
        }

        Q_FALLTHROUGH();
        // lines that start with : might have a constructor initializer list
    case '<':
    case '>': {
        // Electric if at line beginning (after space indentation)
        for (int i = 0, len = text.size(); i < len; ++i) {
            if (!text.at(i).isSpace())
                return text.at(i) == ch;
        }
        return false;
    }
    }

    return true;
}

void CppQtStyleIndenter::indentBlock(const QTextBlock &block,
                                     const QChar &typedChar,
                                     const TabSettings &tabSettings,
                                     int /*cursorPositionInEditor*/)
{
    QtStyleCodeFormatter codeFormatter(tabSettings, codeStyleSettings());

    codeFormatter.updateStateUntil(block);
    if (codeFormatter.isInRawStringLiteral(block))
        return;
    int indent;
    int padding;
    codeFormatter.indentFor(block, &indent, &padding);

    if (isElectricCharacter(typedChar)) {
        // : should not be electric for labels
        if (!isElectricInLine(typedChar, block.text()))
            return;

        // only reindent the current line when typing electric characters if the
        // indent is the same it would be if the line were empty
        int newlineIndent;
        int newlinePadding;
        codeFormatter.indentForNewLineAfter(block.previous(), &newlineIndent, &newlinePadding);
        if (tabSettings.indentationColumn(block.text()) != newlineIndent + newlinePadding)
            return;
    }

    tabSettings.indentLine(block, indent + padding, padding);
}

void CppQtStyleIndenter::indent(const QTextCursor &cursor,
                                const QChar &typedChar,
                                const TabSettings &tabSettings,
                                int /*cursorPositionInEditor*/)
{
    if (cursor.hasSelection()) {
        QTextBlock block = m_doc->findBlock(cursor.selectionStart());
        const QTextBlock end = m_doc->findBlock(cursor.selectionEnd()).next();

        QtStyleCodeFormatter codeFormatter(tabSettings, codeStyleSettings());
        codeFormatter.updateStateUntil(block);

        QTextCursor tc = cursor;
        tc.beginEditBlock();
        do {
            if (!codeFormatter.isInRawStringLiteral(block)) {
                int indent;
                int padding;
                codeFormatter.indentFor(block, &indent, &padding);
                tabSettings.indentLine(block, indent + padding, padding);
            }
            codeFormatter.updateLineStateChange(block);
            block = block.next();
        } while (block.isValid() && block != end);
        tc.endEditBlock();
    } else {
        indentBlock(cursor.block(), typedChar, tabSettings);
    }
}

void CppQtStyleIndenter::setCodeStylePreferences(ICodeStylePreferences *preferences)
{
    auto cppCodeStylePreferences = dynamic_cast<CppCodeStylePreferences *>(preferences);
    if (cppCodeStylePreferences)
        m_cppCodeStylePreferences = cppCodeStylePreferences;
}

void CppQtStyleIndenter::invalidateCache()
{
    QtStyleCodeFormatter formatter;
    formatter.invalidateCache(m_doc);
}

int CppQtStyleIndenter::indentFor(const QTextBlock &block,
                                  const TabSettings &tabSettings,
                                  int /*cursorPositionInEditor*/)
{
    QtStyleCodeFormatter codeFormatter(tabSettings, codeStyleSettings());

    codeFormatter.updateStateUntil(block);
    int indent;
    int padding;
    codeFormatter.indentFor(block, &indent, &padding);

    return indent;
}

int CppQtStyleIndenter::visualIndentFor(const QTextBlock &block,
                                        const TabSettings &tabSettings)
{
    return indentFor(block, tabSettings);
}

CppCodeStyleSettings CppQtStyleIndenter::codeStyleSettings() const
{
    if (m_cppCodeStylePreferences)
        return m_cppCodeStylePreferences->currentCodeStyleSettings();
    return {};
}

IndentationForBlock CppQtStyleIndenter::indentationForBlocks(
    const QList<QTextBlock> &blocks,
    const TabSettings &tabSettings,
    int /*cursorPositionInEditor*/)
{
    QtStyleCodeFormatter codeFormatter(tabSettings, codeStyleSettings());

    codeFormatter.updateStateUntil(blocks.last());

    IndentationForBlock ret;
    for (const QTextBlock &block : blocks) {
        int indent;
        int padding;
        codeFormatter.indentFor(block, &indent, &padding);
        ret.insert(block.blockNumber(), indent);
    }
    return ret;
}

} // namespace Internal

Indenter *createCppQtStyleIndenter(QTextDocument *doc)
{
    return new Internal::CppQtStyleIndenter(doc);
}

} // namespace CppEditor
