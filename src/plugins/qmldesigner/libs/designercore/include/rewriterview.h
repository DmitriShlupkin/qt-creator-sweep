// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmldesignercorelib_global.h"
#include "abstractview.h"
#include "documentmessage.h"
#include "rewritertransaction.h"

#include <QTimer>
#include <QUrl>

#include <functional>
#include <memory>
#include <qmljs/qmljsdocument.h>

namespace QmlJS {
class Document;
class ScopeChain;
}

namespace QmlDesigner {

class TextModifier;

namespace Internal {

class TextToModelMerger;
class ModelToTextMerger;
class ModelNodePositionStorage;

} //Internal

struct QmlTypeData
{
    QString superClassName;
    QString importUrl;
    QString versionString;
    QString cppClassName;
    QString typeName;
    bool isSingleton = false;
    bool isCppType = false;
};

enum class InstantQmlTextUpdate { No, Yes };

class QMLDESIGNERCORE_EXPORT RewriterView : public AbstractView
{
    Q_OBJECT

public:
    enum DifferenceHandling {
        Validate,
        Amend
    };

public:
    RewriterView(ExternalDependenciesInterface &externalDependencies,
                 DifferenceHandling differenceHandling = RewriterView::Amend,
                 InstantQmlTextUpdate instantQmlTextUpdate = InstantQmlTextUpdate::No);
    ~RewriterView() override;

    void modelAttached(Model *model) override;
    void modelAboutToBeDetached(Model *model) override;
    void nodeCreated(const ModelNode &createdNode) override;
    void nodeRemoved(const ModelNode &removedNode, const NodeAbstractProperty &parentProperty, PropertyChangeFlags propertyChange) override;
    void propertiesAboutToBeRemoved(const QList<AbstractProperty>& propertyList) override;
    void propertiesRemoved(const QList<AbstractProperty>& propertyList) override;
    void variantPropertiesChanged(const QList<VariantProperty>& propertyList, PropertyChangeFlags propertyChange) override;
    void bindingPropertiesChanged(const QList<BindingProperty>& propertyList, PropertyChangeFlags propertyChange) override;
    void signalHandlerPropertiesChanged(const QVector<SignalHandlerProperty>& propertyList,PropertyChangeFlags propertyChange) override;
    void signalDeclarationPropertiesChanged(const QVector<SignalDeclarationProperty>& propertyList, PropertyChangeFlags propertyChange) override;
    void nodeReparented(const ModelNode &node, const NodeAbstractProperty &newPropertyParent,
                        const NodeAbstractProperty &oldPropertyParent,
                        AbstractView::PropertyChangeFlags propertyChange) override;
    void nodeIdChanged(const ModelNode& node, const QString& newId, const QString& oldId) override;
    void nodeOrderChanged(const NodeListProperty &listProperty,
                          const ModelNode &movedNode,
                          int /*oldIndex*/) override;
    void nodeOrderChanged(const NodeListProperty &listProperty) override;
    void rootNodeTypeChanged(const QString &type, int majorVersion, int minorVersion) override;
    void nodeTypeChanged(const ModelNode& node, const TypeName &type, int majorVersion, int minorVersion) override;

    void rewriterBeginTransaction() override;
    void rewriterEndTransaction() override;

    void importsChanged(const Imports &addedImports, const Imports &removedImports) override;

    TextModifier *textModifier() const;
    void setTextModifier(TextModifier *textModifier);
    QString textModifierContent() const;

    void reactivateTextModifierChangeSignals();
    void deactivateTextModifierChangeSignals();

    void auxiliaryDataChanged(const ModelNode &node,
                              AuxiliaryDataKeyView key,
                              const QVariant &data) override;

    Internal::ModelNodePositionStorage *positionStorage() const;

    QList<DocumentMessage> warnings() const;
    QList<DocumentMessage> errors() const;
    void clearErrorAndWarnings();
    void setErrors(const QList<DocumentMessage> &errors);
    void setWarnings(const QList<DocumentMessage> &warnings);
    void setIncompleteTypeInformation(bool b);
    bool hasIncompleteTypeInformation() const;
    void addError(const DocumentMessage &error);

    void enterErrorState(const QString &errorMessage);
    void leaveErrorState() { m_rewritingErrorMessage.clear(); }
    void resetToLastCorrectQml();

    QMap<ModelNode, QString> extractText(const QList<ModelNode> &nodes) const;
    int nodeOffset(const ModelNode &node) const;
    int nodeLength(const ModelNode &node) const;
    int firstDefinitionInsideOffset(const ModelNode &node) const;
    int firstDefinitionInsideLength(const ModelNode &node) const;
    bool modificationGroupActive();
    ModelNode nodeAtTextCursorPosition(int cursorPosition) const;

    bool renameId(const QString& oldId, const QString& newId);

    QmlJS::Document::Ptr document() const;

#ifndef QDS_USE_PROJECTSTORAGE
    const QmlJS::ScopeChain *scopeChain() const;
#endif

    QString convertTypeToImportAlias(const QString &type) const;

    bool checkSemanticErrors() const { return m_checkSemanticErrors; }

    void setCheckSemanticErrors(bool b) { m_checkSemanticErrors = b; }

    bool checkLinkErrors() const { return m_checkLinkErrors; }

    void setCheckLinkErrors(bool b) { m_checkLinkErrors = b; }

    QStringList importDirectories() const;

    QSet<QPair<QString, QString> > qrcMapping() const;

    void moveToComponent(const ModelNode &modelNode);

    QStringList autoComplete(const QString &text, int pos, bool explicitComplete = true);

#ifndef QDS_USE_PROJECTSTORAGE
    QList<QmlTypeData> getQMLTypes() const;
#endif

    void setWidgetStatusCallback(std::function<void(bool)> setWidgetStatusCallback);

    void qmlTextChanged();
    void delayedSetup();

    void writeAuxiliaryData();
    void restoreAuxiliaryData();

    QString getRawAuxiliaryData() const;
    QString auxiliaryDataAsQML() const;

    ModelNode getNodeForCanonicalIndex(int index);

    void setAllowComponentRoot(bool allow);
    bool allowComponentRoot() const;

    void resetPossibleImports();

    bool possibleImportsEnabled() const;
    void setPossibleImportsEnabled(bool b);

    void forceAmend();

#ifndef QDS_USE_PROJECTSTORAGE
    bool isDocumentRewriterView() const;
    void setIsDocumentRewriterView(bool b);
#endif

    void setRemoveImports(bool removeImports);

signals:
    void modelInterfaceProjectUpdated();

protected: // functions
    void importsAdded(const Imports &imports);
    void importsRemoved(const Imports &imports);

    Internal::ModelToTextMerger *modelToTextMerger() const;
    Internal::TextToModelMerger *textToModelMerger() const;
    bool isModificationGroupActive() const;
    void setModificationGroupActive(bool active);
    void applyModificationGroupChanges();
    void applyChanges();
    void amendQmlText();
    void notifyErrorsAndWarnings(const QList<DocumentMessage> &errors);

private: //variables
    ModelNode nodeAtTextCursorPositionHelper(const ModelNode &root, int cursorPosition) const;
    void setupCanonicalHashes() const;
#ifndef QDS_USE_PROJECTSTORAGE
    void handleLibraryInfoUpdate();
    void handleProjectUpdate();
#endif
    bool inErrorState() const { return !m_rewritingErrorMessage.isEmpty(); }

    QPointer<TextModifier> m_textModifier;
    int transactionLevel = 0;
    bool m_modificationGroupActive = false;
    bool m_checkSemanticErrors = true;
    bool m_checkLinkErrors = true;

    DifferenceHandling m_differenceHandling;
    std::unique_ptr<Internal::ModelNodePositionStorage> m_positionStorage;
    std::unique_ptr<Internal::ModelToTextMerger> m_modelToTextMerger;
    std::unique_ptr<Internal::TextToModelMerger> m_textToModelMerger;
    QList<DocumentMessage> m_errors;
    QList<DocumentMessage> m_warnings;
    RewriterTransaction m_removeDefaultPropertyTransaction;
    QString m_rewritingErrorMessage;
    QString m_lastCorrectQmlSource;
    QTimer m_amendTimer;
    InstantQmlTextUpdate m_instantQmlTextUpdate = InstantQmlTextUpdate::No;
    std::function<void(bool)> m_setWidgetStatusCallback;
    bool m_hasIncompleteTypeInformation = false;
    bool m_restoringAuxData = false;
    bool m_modelAttachPending = false;
    bool m_allowComponentRoot = false;
    bool m_possibleImportsEnabled = true;

#ifndef QDS_USE_PROJECTSTORAGE
    bool m_isDocumentRewriterView = false;
#endif

    mutable QHash<int, ModelNode> m_canonicalIntModelNode;
    mutable QHash<ModelNode, int> m_canonicalModelNodeInt;
};

} //QmlDesigner
