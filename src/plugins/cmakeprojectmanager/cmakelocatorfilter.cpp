// Copyright (C) 2016 Kläralvdalens Datakonsult AB, a KDAB Group company.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakelocatorfilter.h"

#include "cmakebuildstep.h"
#include "cmakebuildsystem.h"
#include "cmakeproject.h"
#include "cmakeprojectmanagertr.h"

#include <coreplugin/locator/ilocatorfilter.h>

#include <projectexplorer/buildmanager.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>

#include <utils/algorithm.h>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager::Internal {

using BuildAcceptor = std::function<void(const FilePath &, const QString &)>;

// CMakeBuildTargetFilter

static LocatorMatcherTasks cmakeMatchers(const BuildAcceptor &acceptor)
{
    using namespace Tasking;

    Storage<LocatorStorage> storage;

    const auto onSetup = [storage, acceptor] {
        const QString input = storage->input();
        const QRegularExpression regexp
            = ILocatorFilter::createRegExp(input, ILocatorFilter::caseSensitivity(input));
        if (!regexp.isValid())
            return;
        LocatorFilterEntries entries[int(ILocatorFilter::MatchLevel::Count)];

        const QList<Project *> projects = ProjectManager::projects();
        for (Project *project : projects) {
            const auto cmakeProject = qobject_cast<const CMakeProject *>(project);
            if (!cmakeProject || !cmakeProject->activeTarget())
                continue;
            const auto bs = qobject_cast<CMakeBuildSystem *>(
                cmakeProject->activeTarget()->buildSystem());
            if (!bs)
                continue;

            const QList<CMakeBuildTarget> buildTargets = bs->buildTargets();
            for (const CMakeBuildTarget &target : buildTargets) {
                if (CMakeBuildSystem::filteredOutTarget(target))
                    continue;
                const QRegularExpressionMatch match = regexp.match(target.title);
                if (match.hasMatch()) {
                    const FilePath projectPath = cmakeProject->projectFilePath();
                    const QString displayName = target.title;
                    LocatorFilterEntry entry;
                    entry.displayName = displayName;
                    if (acceptor) {
                        entry.acceptor = [projectPath, displayName, acceptor] {
                            acceptor(projectPath, displayName);
                            return AcceptResult();
                        };
                    }
                    bool realTarget = false;
                    if (!target.backtrace.isEmpty() && target.targetType != UtilityType) {
                        const FilePath path = target.backtrace.last().path;
                        const int line = target.backtrace.last().line;
                        entry.linkForEditor = {path, line};
                        entry.extraInfo = path.shortNativePath();
                        realTarget = true;
                    } else {
                        entry.extraInfo = projectPath.shortNativePath();
                    }
                    entry.highlightInfo = ILocatorFilter::highlightInfo(match);
                    entry.filePath = cmakeProject->projectFilePath();
                    if (acceptor || realTarget) {
                        if (match.capturedStart() == 0)
                            entries[int(ILocatorFilter::MatchLevel::Best)].append(entry);
                        else if (match.lastCapturedIndex() == 1)
                            entries[int(ILocatorFilter::MatchLevel::Better)].append(entry);
                        else
                            entries[int(ILocatorFilter::MatchLevel::Good)].append(entry);
                    }
                }
            }
        }
        storage->reportOutput(
            std::accumulate(std::begin(entries), std::end(entries), LocatorFilterEntries()));
    };
    return {{Sync(onSetup), storage}};
}

static void setupFilter(ILocatorFilter *filter)
{
    const auto projectListUpdated = [filter] {
        filter->setEnabled(Utils::contains(ProjectManager::projects(),
                           [](Project *p) { return qobject_cast<CMakeProject *>(p); }));
    };
    QObject::connect(ProjectManager::instance(), &ProjectManager::projectAdded,
                     filter, projectListUpdated);
    QObject::connect(ProjectManager::instance(), &ProjectManager::projectRemoved,
                     filter, projectListUpdated);
}

static void buildAcceptor(const FilePath &projectPath, const QString &displayName)
{
    // Get the project containing the target selected
    const auto cmakeProject = qobject_cast<CMakeProject *>(
        Utils::findOrDefault(ProjectManager::projects(), [projectPath](Project *p) {
            return p->projectFilePath() == projectPath;
        }));
    if (!cmakeProject || !cmakeProject->activeTarget()
        || !cmakeProject->activeTarget()->activeBuildConfiguration())
        return;

    if (BuildManager::isBuilding(cmakeProject))
        BuildManager::cancel();

    // Find the make step
    const BuildStepList *buildStepList =
        cmakeProject->activeTarget()->activeBuildConfiguration()->buildSteps();
    const auto buildStep = buildStepList->firstOfType<CMakeBuildStep>();
    if (!buildStep)
        return;

    // Change the make step to build only the given target
    const QStringList oldTargets = buildStep->buildTargets();
    buildStep->setBuildTargets({displayName});

    // Build
    BuildManager::buildProjectWithDependencies(cmakeProject);
    buildStep->setBuildTargets(oldTargets);
}

class CMakeBuildTargetFilter final : ILocatorFilter
{
public:
    CMakeBuildTargetFilter()
    {
        setId("Build CMake target");
        setDisplayName(Tr::tr("Build CMake Target"));
        setDescription(Tr::tr("Builds a target of any open CMake project."));
        setDefaultShortcutString("cm");
        setPriority(High);
        setupFilter(this);
    }

private:
    LocatorMatcherTasks matchers() final { return cmakeMatchers(&buildAcceptor); }
};

// OpenCMakeTargetLocatorFilter

class CMakeOpenTargetFilter final : ILocatorFilter
{
public:
    CMakeOpenTargetFilter()
    {
        setId("Open CMake target definition");
        setDisplayName(Tr::tr("Open CMake Target"));
        setDescription(Tr::tr("Locates the definition of a target of any open CMake project."));
        setDefaultShortcutString("cmo");
        setPriority(Medium);
        setupFilter(this);
    }

private:
    LocatorMatcherTasks matchers() final { return cmakeMatchers({}); }
};

// Setup

void setupCMakeLocatorFilters()
{
    static CMakeBuildTargetFilter theCMakeBuildTargetFilter;
    static CMakeOpenTargetFilter theCMakeOpenTargetFilter;
}

} // CMakeProjectManager::Internal
