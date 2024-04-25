// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "contentlibraryusermodel.h"

#include "contentlibrarybundleimporter.h"
#include "contentlibrarymaterial.h"
#include "contentlibrarymaterialscategory.h"
#include "contentlibrarytexture.h"
#include "contentlibrarywidget.h"

#include <designerpaths.h>
#include <imageutils.h>
#include <qmldesignerplugin.h>

#include <utils/algorithm.h>
#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QUrl>

namespace QmlDesigner {

ContentLibraryUserModel::ContentLibraryUserModel(ContentLibraryWidget *parent)
    : QAbstractListModel(parent)
    , m_widget(parent)
{
    m_userCategories = {tr("Materials"), tr("Textures")/*, tr("3D"), tr("Effects"), tr("2D components")*/}; // TODO

    loadMaterialBundle();
    loadTextureBundle();
}

int ContentLibraryUserModel::rowCount(const QModelIndex &) const
{
    return m_userCategories.size();
}

QVariant ContentLibraryUserModel::data(const QModelIndex &index, int role) const
{
    QTC_ASSERT(index.isValid() && index.row() < m_userCategories.size(), return {});
    QTC_ASSERT(roleNames().contains(role), return {});

    if (role == NameRole)
        return m_userCategories.at(index.row());

    if (role == ItemsRole) {
        if (index.row() == 0)
            return QVariant::fromValue(m_userMaterials);
        if (index.row() == 1)
            return QVariant::fromValue(m_userTextures);
        if (index.row() == 2)
            return QVariant::fromValue(m_user3DItems);
        if (index.row() == 3)
            return QVariant::fromValue(m_userEffects);
    }

    if (role == VisibleRole)
        return true; // TODO

    return {};
}

bool ContentLibraryUserModel::isValidIndex(int idx) const
{
    return idx > -1 && idx < rowCount();
}

void ContentLibraryUserModel::updateIsEmpty()
{
    bool anyMatVisible = Utils::anyOf(m_userMaterials, [&](ContentLibraryMaterial *mat) {
        return mat->visible();
    });

    bool newEmpty = !anyMatVisible || !m_widget->hasMaterialLibrary() || !hasRequiredQuick3DImport();

    if (newEmpty != m_isEmpty) {
        m_isEmpty = newEmpty;
        emit isEmptyChanged();
    }
}

void ContentLibraryUserModel::addMaterial(const QString &name, const QString &qml,
                                          const QUrl &icon, const QStringList &files)
{
    auto libMat = new ContentLibraryMaterial(this, name, qml, qmlToModule(qml), icon, files,
                                             Paths::bundlesPathSetting().append("/User/materials"));

    m_userMaterials.append(libMat);
    int matSectionIdx = 0;
    emit dataChanged(index(matSectionIdx, 0), index(matSectionIdx, 0));
}

void ContentLibraryUserModel::removeTexture(ContentLibraryTexture *tex)
{
    // remove resources
    Utils::FilePath::fromString(tex->texturePath()).removeFile();
    Utils::FilePath::fromString(tex->iconPath()).removeFile();

    // remove from model
    m_userTextures.removeOne(tex);
    tex->deleteLater();

    // update model
    int texSectionIdx = 1;
    emit dataChanged(index(texSectionIdx), index(texSectionIdx));
}

// returns unique library material's name and qml component
QPair<QString, QString> ContentLibraryUserModel::getUniqueLibMaterialNameAndQml(const QString &matName) const
{
    QTC_ASSERT(!m_bundleObj.isEmpty(), return {});

    const QJsonObject matsObj = m_bundleObj.value("materials").toObject();
    const QStringList matNames = matsObj.keys();

    QStringList matQmls;
    for (const QString &matName : matNames)
        matQmls.append(matsObj.value(matName).toObject().value("qml").toString().chopped(4)); // remove .qml

    QString retName = matName.isEmpty() ? "Material" : matName;
    retName = retName.trimmed();

    QString retQml = retName;
    retQml.remove(' ');
    if (retQml.at(0).isLower())
        retQml[0] = retQml.at(0).toUpper();
    retQml.prepend("My");

    int num = 1;
    if (matNames.contains(retName) || matQmls.contains(retQml)) {
        while (matNames.contains(retName + QString::number(num))
             || matQmls.contains(retQml + QString::number(num))) {
            ++num;
        }

        retName += QString::number(num);
        retQml += QString::number(num);
    }

    return {retName, retQml + ".qml"};
}

TypeName ContentLibraryUserModel::qmlToModule(const QString &qmlName) const
{
    return QLatin1String("%1.%2.%3").arg(QmlDesignerPlugin::instance()->documentManager()
                                             .generatedComponentUtils().componentBundlesTypePrefix(),
                                         m_bundleId,
                                         qmlName.chopped(4)).toLatin1(); // chopped(4): remove .qml
}

QHash<int, QByteArray> ContentLibraryUserModel::roleNames() const
{
    static const QHash<int, QByteArray> roles {
        {NameRole, "categoryName"},
        {VisibleRole, "categoryVisible"},
        {ItemsRole, "categoryItems"}
    };
    return roles;
}

void ContentLibraryUserModel::createImporter(const QString &bundlePath, const QString &bundleId,
                                             const QStringList &sharedFiles)
{
    m_importer = new Internal::ContentLibraryBundleImporter(bundlePath, bundleId, sharedFiles);
#ifdef QDS_USE_PROJECTSTORAGE
    connect(m_importer,
            &Internal::ContentLibraryBundleImporter::importFinished,
            this,
            [&](const QmlDesigner::TypeName &typeName) {
                m_importerRunning = false;
                emit importerRunningChanged();
                if (typeName.size())
                    emit bundleMaterialImported(typeName);
            });
#else
    connect(m_importer,
            &Internal::ContentLibraryBundleImporter::importFinished,
            this,
            [&](const QmlDesigner::NodeMetaInfo &metaInfo) {
                m_importerRunning = false;
                emit importerRunningChanged();
                if (metaInfo.isValid())
                    emit bundleMaterialImported(metaInfo);
            });
#endif

    connect(m_importer, &Internal::ContentLibraryBundleImporter::unimportFinished, this,
            [&](const QmlDesigner::NodeMetaInfo &metaInfo) {
                Q_UNUSED(metaInfo)
                m_importerRunning = false;
                emit importerRunningChanged();
                emit bundleMaterialUnimported(metaInfo);
            });

    resetModel();
    updateIsEmpty();
}

QJsonObject &ContentLibraryUserModel::bundleJsonObjectRef()
{
    return m_bundleObj;
}

void ContentLibraryUserModel::loadMaterialBundle()
{
    if (m_matBundleExists)
        return;

    QDir bundleDir{Paths::bundlesPathSetting() + "/User/materials"};
    bundleDir.mkpath(".");

    if (m_bundleObj.isEmpty()) {
        auto jsonFilePath = Utils::FilePath::fromString(bundleDir.filePath("user_materials_bundle.json"));
        if (!jsonFilePath.exists()) {
            QString jsonContent = "{\n";
            jsonContent += "    \"id\": \"UserMaterialBundle\",\n";
            jsonContent += "    \"materials\": {\n";
            jsonContent += "    }\n";
            jsonContent += "}";
            jsonFilePath.writeFileContents(jsonContent.toLatin1());
        }

        QFile jsonFile(jsonFilePath.path());
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            qWarning("Couldn't open user_materials_bundle.json");
            return;
        }

        QJsonDocument matBundleJsonDoc = QJsonDocument::fromJson(jsonFile.readAll());
        if (matBundleJsonDoc.isNull()) {
            qWarning("Invalid user_materials_bundle.json file");
            return;
        } else {
            m_bundleObj = matBundleJsonDoc.object();
        }
    }

    m_bundleId = m_bundleObj.value("id").toString();

    // parse materials
    const QJsonObject matsObj = m_bundleObj.value("materials").toObject();
    const QStringList materialNames = matsObj.keys();
    for (const QString &matName : materialNames) {
        const QJsonObject matObj = matsObj.value(matName).toObject();

        QStringList files;
        const QJsonArray assetsArr = matObj.value("files").toArray();
        for (const auto /*QJson{Const,}ValueRef*/ &asset : assetsArr)
            files.append(asset.toString());

        QUrl icon = QUrl::fromLocalFile(bundleDir.filePath(matObj.value("icon").toString()));
        QString qml = matObj.value("qml").toString();

        TypeName type = qmlToModule(qml);

        auto userMat = new ContentLibraryMaterial(this, matName, qml, type, icon, files,
                                                  bundleDir.path(), "");

        m_userMaterials.append(userMat);
    }

    QStringList sharedFiles;
    const QJsonArray sharedFilesArr = m_bundleObj.value("sharedFiles").toArray();
    for (const auto /*QJson{Const,}ValueRef*/ &file : sharedFilesArr)
        sharedFiles.append(file.toString());

    createImporter(bundleDir.path(), m_bundleId, sharedFiles);

    m_matBundleExists = true;
    emit matBundleExistsChanged();
}

void ContentLibraryUserModel::loadTextureBundle()
{
    if (!m_userTextures.isEmpty())
        return;

    QDir bundleDir{Paths::bundlesPathSetting() + "/User/textures"};
    bundleDir.mkpath(".");
    bundleDir.mkdir("icons");

    const QFileInfoList fileInfos = bundleDir.entryInfoList(QDir::Files);
    for (const QFileInfo &fileInfo : fileInfos) {
        auto iconFileInfo = QFileInfo(fileInfo.path().append("/icons/").append(fileInfo.fileName()));
        QPair<QSize, qint64> info = ImageUtils::imageInfo(fileInfo.path());
        QString dirPath = fileInfo.path();
        QString suffix = '.' + fileInfo.suffix();
        QSize imgDims = info.first;
        qint64 imgFileSize = info.second;

        auto tex = new ContentLibraryTexture(this, iconFileInfo, dirPath, suffix, imgDims, imgFileSize);
        m_userTextures.append(tex);
    }

    int texSectionIdx = 1;
    emit dataChanged(index(texSectionIdx), index(texSectionIdx));
}

bool ContentLibraryUserModel::hasRequiredQuick3DImport() const
{
    return m_widget->hasQuick3DImport() && m_quick3dMajorVersion == 6 && m_quick3dMinorVersion >= 3;
}

bool ContentLibraryUserModel::matBundleExists() const
{
    return m_matBundleExists;
}

Internal::ContentLibraryBundleImporter *ContentLibraryUserModel::bundleImporter() const
{
    return m_importer;
}

void ContentLibraryUserModel::setSearchText(const QString &searchText)
{
    QString lowerSearchText = searchText.toLower();

    if (m_searchText == lowerSearchText)
        return;

    m_searchText = lowerSearchText;

    for (ContentLibraryMaterial *mat : std::as_const(m_userMaterials))
        mat->filter(m_searchText);

    updateIsEmpty();
}

void ContentLibraryUserModel::updateImportedState(const QStringList &importedMats)
{
    bool changed = false;

    for (ContentLibraryMaterial *mat : std::as_const(m_userMaterials))
        changed |= mat->setImported(importedMats.contains(mat->qml().chopped(4)));

    if (changed)
        resetModel();
}

void ContentLibraryUserModel::setQuick3DImportVersion(int major, int minor)
{
    bool oldRequiredImport = hasRequiredQuick3DImport();

    m_quick3dMajorVersion = major;
    m_quick3dMinorVersion = minor;

    bool newRequiredImport = hasRequiredQuick3DImport();

    if (oldRequiredImport == newRequiredImport)
        return;

    emit hasRequiredQuick3DImportChanged();

    updateIsEmpty();
}

void ContentLibraryUserModel::resetModel()
{
    beginResetModel();
    endResetModel();
}

void ContentLibraryUserModel::applyToSelected(ContentLibraryMaterial *mat, bool add)
{
    emit applyToSelectedTriggered(mat, add);
}

void ContentLibraryUserModel::addToProject(ContentLibraryMaterial *mat)
{
    QString err = m_importer->importComponent(mat->qml(), mat->files());

    if (err.isEmpty()) {
        m_importerRunning = true;
        emit importerRunningChanged();
    } else {
        qWarning() << __FUNCTION__ << err;
    }
}

void ContentLibraryUserModel::removeFromProject(ContentLibraryMaterial *mat)
{
    emit bundleMaterialAboutToUnimport(mat->type());

     QString err = m_importer->unimportComponent(mat->qml());

    if (err.isEmpty()) {
        m_importerRunning = true;
        emit importerRunningChanged();
    } else {
        qWarning() << __FUNCTION__ << err;
    }
}

bool ContentLibraryUserModel::hasModelSelection() const
{
    return m_hasModelSelection;
}

void ContentLibraryUserModel::setHasModelSelection(bool b)
{
    if (b == m_hasModelSelection)
        return;

    m_hasModelSelection = b;
    emit hasModelSelectionChanged();
}

} // namespace QmlDesigner
