// Copyright (C) 2016 Lorenz Haas
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "configurationeditor.h"

#include "beautifiertool.h"

#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>
#include <utils/qtcassert.h>

#include <QAbstractItemView>
#include <QCompleter>
#include <QScrollBar>
#include <QStringListModel>

namespace Beautifier::Internal {

ConfigurationSyntaxHighlighter::ConfigurationSyntaxHighlighter(QTextDocument *parent) :
    QSyntaxHighlighter(parent)
{
    const TextEditor::FontSettings fs = TextEditor::TextEditorSettings::fontSettings();
    m_formatKeyword = fs.toTextCharFormat(TextEditor::C_FIELD);
    m_formatComment = fs.toTextCharFormat(TextEditor::C_COMMENT);
}

void ConfigurationSyntaxHighlighter::setKeywords(const QStringList &keywords)
{
    if (keywords.isEmpty())
        return;

    // Check for empty keywords since they can cause an endless loop in highlightBlock().
    QStringList pattern;
    for (const QString &word : keywords) {
        if (!word.isEmpty())
            pattern << QRegularExpression::escape(word);
    }

    m_expressionKeyword.setPattern("(?:\\s|^)(" + pattern.join('|') + ")(?=\\s|\\:|\\=|\\,|$)");
}

void ConfigurationSyntaxHighlighter::highlightBlock(const QString &text)
{
    QRegularExpressionMatchIterator it = m_expressionKeyword.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_formatKeyword);
    }

    static const QRegularExpression expressionComment("#[^\\n]*");
    it = expressionComment.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_formatComment);
    }
}

ConfigurationEditor::ConfigurationEditor(QWidget *parent) :
    QPlainTextEdit(parent),
    m_completer(new QCompleter(this)),
    m_model(new QStringListModel(QStringList(), m_completer)),
    m_highlighter(new ConfigurationSyntaxHighlighter(document()))
{
    m_completer->setModel(m_model);
    m_completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    m_completer->setWrapAround(false);
    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->popup()->installEventFilter(this);

    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, &ConfigurationEditor::insertCompleterText);
    connect(this, &ConfigurationEditor::cursorPositionChanged,
            this, &ConfigurationEditor::updateDocumentation);
}

void ConfigurationEditor::setSettings(AbstractSettings *settings)
{
    QTC_ASSERT(settings, return);
    m_settings = settings;

    QStringList keywords = m_settings->options();
    m_highlighter->setKeywords(keywords);
    keywords << m_settings->completerWords();
    keywords.sort(Qt::CaseInsensitive);
    m_model->setStringList(keywords);
}

// Workaround for handling "ESC" right when popup is shown.
bool ConfigurationEditor::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        auto key = static_cast<const QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            event->accept();
            m_completer->popup()->hide();
            return true;
        }
    }
    return QObject::eventFilter(object, event);
}

void ConfigurationEditor::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();

    if (key == Qt::Key_Escape) {
        event->ignore();
        return;
    }

    if (m_completer->popup()->isVisible()) {
        switch (key) {
        case Qt::Key_Backtab:
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab:
            event->ignore();
            return;
        default:
            break;
        }
    }

    const bool isShortcut = (event->modifiers() & Qt::ControlModifier) && key == Qt::Key_Space;
    if (!isShortcut)
        QPlainTextEdit::keyPressEvent(event);

    const int cursorPosition = textCursor().position();
    const QTextCursor cursor = cursorForTextUnderCursor();
    const QString prefix = cursor.selectedText();

    if (!isShortcut && (prefix.length() < 2 || cursorPosition != cursor.position())) {
        m_completer->popup()->hide();
        return;
    }

    if (prefix != m_completer->completionPrefix()) {
        m_completer->setCompletionPrefix(prefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }

    if (m_completer->completionCount() == 1 && prefix == m_completer->currentCompletion()) {
        m_completer->popup()->hide();
        return;
    }

    QRect popupRect = cursorRect();
    popupRect.setWidth(m_completer->popup()->sizeHintForColumn(0)
                       + m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(popupRect);
}

// Hack because '-' and '=' are treated as End/StartOfWord
QTextCursor ConfigurationEditor::cursorForTextUnderCursor(QTextCursor tc) const
{
    if (tc.isNull())
        tc = textCursor();

    tc.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
    QChar ch = document()->characterAt(tc.position() - 1);
    while (!(ch.isNull() || ch.isSpace() || ch == ':' || ch == ',')) {
        tc.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor);
        ch = document()->characterAt(tc.position() - 1);
    }
    tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    ch = document()->characterAt(tc.position());
    while (!(ch.isNull() || ch.isSpace() || ch == ':' || ch == ',')) {
        tc.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        ch = document()->characterAt(tc.position());
    }
    return tc;
}

void ConfigurationEditor::insertCompleterText(const QString &text)
{
    QTextCursor tc = textCursor();
    // Replace entire word to get case sensitivity right.
    tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor,
                    m_completer->completionPrefix().length());
    tc.insertText(text);
    setTextCursor(tc);
}

void ConfigurationEditor::updateDocumentation()
{
    QTC_CHECK(m_settings);
    QTextCursor cursor = textCursor();

    QString word = cursorForTextUnderCursor(cursor).selectedText();
    if (word == m_lastDocumentation)
        return;

    QString doc = m_settings->documentation(word);
    if (!doc.isEmpty()) {
        m_lastDocumentation = word;
        emit documentationChanged(word, doc);
        return;
    }

    // If the documentation was empty, then try to use the line's first word or the first word
    // in front of a colon for providing a documentation.
    cursor.movePosition(QTextCursor::PreviousWord);
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    const int pos = cursor.selectedText().lastIndexOf(',');
    if (-1 != pos) {
        cursor.setPosition(cursor.position() + pos);
        cursor.movePosition(QTextCursor::NextWord);
    }
    word = cursorForTextUnderCursor(cursor).selectedText();

    if (word == m_lastDocumentation)
        return;

    doc = m_settings->documentation(word);
    if (doc.isEmpty())
        return;

    m_lastDocumentation = word;
    emit documentationChanged(word, doc);
}

} // Beautifier::Internal
