// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/textdocument.h>
#include <utils/guard.h>

#include <QPointer>

QT_BEGIN_NAMESPACE
class QDesignerFormWindowInterface;
QT_END_NAMESPACE

namespace Designer::Internal {

class ResourceHandler;

class FormWindowFile : public TextEditor::TextDocument
{
    Q_OBJECT

public:
    explicit FormWindowFile(QDesignerFormWindowInterface *form, QObject *parent = nullptr);
    ~FormWindowFile() override { }

    // IDocument
    Utils::Result<> open(const Utils::FilePath &filePath,
                         const Utils::FilePath &realFilePath) override;
    QByteArray contents() const override;
    Utils::Result<> setContents(const QByteArray &contents) override;
    bool shouldAutoSave() const override;
    bool isModified() const override;
    bool isSaveAsAllowed() const override;
    Utils::Result<> reload(ReloadFlag flag, ChangeType type) override;
    QString fallbackSaveAsFileName() const override;
    bool supportsEncoding(const Utils::TextEncoding &encoding) const override;

    // Internal
    void setFallbackSaveAsFileName(const QString &fileName);

    Utils::Result<> writeFile(const Utils::FilePath &filePath) const;

    QDesignerFormWindowInterface *formWindow() const;
    void syncXmlFromFormWindow();
    QString formWindowContents() const;
    ResourceHandler *resourceHandler() const;

    void setFilePath(const Utils::FilePath &) override;
    void setShouldAutoSave(bool sad = true) { m_shouldAutoSave = sad; }
    void updateIsModified();

protected:
    Utils::Result<> saveImpl(const Utils::FilePath &filePath, bool autoSave) override;

private:
    void slotFormWindowRemoved(QDesignerFormWindowInterface *w);

    QString m_suggestedName;
    bool m_shouldAutoSave = false;
    // Might actually go out of scope before the IEditor due
    // to deleting the WidgetHost which owns it.
    QPointer<QDesignerFormWindowInterface> m_formWindow;
    bool m_isModified = false;
    ResourceHandler *m_resourceHandler = nullptr;
    Utils::Guard m_modificationChangedGuard;
};

} // namespace Designer::Internal
