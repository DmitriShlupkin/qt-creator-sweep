QtcPlugin {
    name: "Debugger"

    Depends { name: "Qt"; submodules: ["network", "widgets"] }

    Depends { name: "Aggregation" }
    Depends { name: "CPlusPlus" }
    Depends { name: "LanguageUtils" }
    Depends { name: "QmlDebug" }
    Depends { name: "QmlJS" }
    Depends { name: "Utils" }

    Depends { name: "Core" }
    Depends { name: "CppEditor" }
    Depends { name: "ProjectExplorer" }
    Depends { name: "QtSupport" }
    Depends { name: "TextEditor" }
    Depends { name: "clang_defines" }

    pluginRecommends: ["BinEditor"]
    pluginTestDepends: ["QmakeProjectManager"]

    cpp.includePaths: base.concat([project.sharedSourcesDir + "/registryaccess"])
    cpp.enableExceptions: true

    Group {
        name: "General"
        files: [
            "breakhandler.cpp", "breakhandler.h",
            "breakpoint.cpp", "breakpoint.h",
            "commonoptionspage.cpp", "commonoptionspage.h",
            "debugger_global.h", "debuggertr.h",
            "debuggeractions.cpp", "debuggeractions.h",
            "debuggerconstants.h",
            "debuggericons.h", "debuggericons.cpp",
            "debuggercore.h",
            "debuggerdialogs.cpp", "debuggerdialogs.h",
            "debuggerengine.cpp", "debuggerengine.h",
            "debuggerinternalconstants.h",
            "debuggeritem.cpp", "debuggeritem.h",
            "debuggeritemmanager.cpp", "debuggeritemmanager.h",
            "debuggerkitaspect.cpp", "debuggerkitaspect.h",
            "debuggermainwindow.cpp", "debuggermainwindow.h",
            "debuggerplugin.cpp",
            "debuggerprotocol.cpp", "debuggerprotocol.h",
            "debuggerrunconfigurationaspect.cpp", "debuggerrunconfigurationaspect.h",
            "debuggerruncontrol.cpp", "debuggerruncontrol.h",
            "debuggersourcepathmappingwidget.cpp", "debuggersourcepathmappingwidget.h",
            "debuggertest.cpp", "debuggertest.h",
            "debuggertooltipmanager.cpp", "debuggertooltipmanager.h",
            "disassembleragent.cpp", "disassembleragent.h",
            "disassemblerlines.cpp", "disassemblerlines.h",
            "enginemanager.cpp", "enginemanager.h",
            "imageviewer.cpp", "imageviewer.h",
            "loadcoredialog.cpp", "loadcoredialog.h",
            "localsandexpressionswindow.cpp", "localsandexpressionswindow.h",
            "logwindow.cpp", "logwindow.h",
            "memoryagent.cpp", "memoryagent.h",
            "moduleshandler.cpp", "moduleshandler.h",
            "outputcollector.cpp", "outputcollector.h",
            "peripheralregisterhandler.cpp", "peripheralregisterhandler.h",
            "procinterrupt.cpp", "procinterrupt.h",
            "registerhandler.cpp", "registerhandler.h",
            "sourceagent.cpp", "sourceagent.h",
            "sourcefileshandler.cpp", "sourcefileshandler.h",
            "sourceutils.cpp", "sourceutils.h",
            "stackframe.cpp", "stackframe.h",
            "stackhandler.cpp", "stackhandler.h",
            "stackwindow.cpp", "stackwindow.h",
            "terminal.cpp", "terminal.h",
            "threaddata.h",
            "threadshandler.cpp", "threadshandler.h",
            "watchdata.cpp", "watchdata.h",
            "watchdelegatewidgets.cpp", "watchdelegatewidgets.h",
            "watchhandler.cpp", "watchhandler.h",
            "watchutils.cpp", "watchutils.h",
            "watchwindow.cpp", "watchwindow.h",
            "simplifytype.cpp", "simplifytype.h",
            "unstartedappwatcherdialog.cpp", "unstartedappwatcherdialog.h"
        ]
    }

    Group {
        name: "cdb"
        prefix: "cdb/"
        files: [
            "cdbengine.cpp", "cdbengine.h",
            "cdboptionspage.cpp", "cdboptionspage.h",
            "cdbparsehelpers.cpp", "cdbparsehelpers.h",
            "stringinputstream.cpp", "stringinputstream.h",
        ]
    }

    Group {
        name: "gdb"
        prefix: "gdb/"
        files: [
            "gdbengine.cpp", "gdbengine.h",
            "gdbsettings.cpp", "gdbsettings.h",
        ]
    }

    Group {
        name: "lldb"
        prefix: "lldb/"
        files: [
            "lldbengine.cpp", "lldbengine.h"
        ]
    }

    Group {
        name: "pdb"
        prefix: "pdb/"
        files: ["pdbengine.cpp", "pdbengine.h"]
    }

    Group {
        name: "dap"
        prefix: "dap/"
        files: [
            "cmakedapengine.cpp", "cmakedapengine.h",
            "dapclient.cpp", "dapclient.h",
            "dapengine.cpp", "dapengine.h",
            "gdbdapengine.cpp", "gdbdapengine.h",
            "lldbdapengine.cpp", "lldbdapengine.h",
            "pydapengine.cpp", "pydapengine.h",
        ]
    }

    Group {
        name: "uvsc"
        prefix: "uvsc/"
        files: [
            "uvscclient.cpp", "uvscclient.h",
            "uvscdatatypes.h",
            "uvscengine.cpp", "uvscengine.h",
            "uvscfunctions.h",
            "uvscutils.cpp", "uvscutils.h",
        ]
    }

    Group {
        name: "QML Debugger"
        prefix: "qml/"
        files: [
            "interactiveinterpreter.cpp", "interactiveinterpreter.h",
            "qmlengine.cpp", "qmlengine.h",
            "qmlengineutils.cpp", "qmlengineutils.h",
            "qmlinspectoragent.cpp", "qmlinspectoragent.h",
            "qmlv8debuggerclientconstants.h"
        ]
    }

    Group {
        name: "Debugger Console"
        prefix: "console/"
        files: [
            "consoleitem.cpp", "consoleitem.h",
            "consoleedit.cpp", "consoleedit.h",
            "consoleitemdelegate.cpp", "consoleitemdelegate.h",
            "consoleitemmodel.cpp", "consoleitemmodel.h",
            "console.cpp", "console.h",
            "consoleproxymodel.cpp", "consoleproxymodel.h",
            "consoleview.cpp", "consoleview.h"
        ]
    }

    Group {
        name: "shared"
        prefix: "shared/"
        files: [
            "cdbsymbolpathlisteditor.cpp",
            "cdbsymbolpathlisteditor.h",
            "coredumputils.cpp", "coredumputils.h",
            "hostutils.cpp", "hostutils.h",
            "peutils.cpp", "peutils.h",
            "symbolpathsdialog.cpp", "symbolpathsdialog.h"
        ]
    }

    Group {
        name: "RegistryAccess"
        condition: qbs.targetOS.contains("windows")
        prefix: project.sharedSourcesDir + "/registryaccess/"
        files: [
            "registryaccess.cpp",
            "registryaccess.h",
        ]
    }

    Group {
        name: "RegisterPostMortem"
        condition: qbs.targetOS.contains("windows")
        files: [
            "registerpostmortemaction.cpp",
            "registerpostmortemaction.h",
        ]
    }

    Properties {
        condition: qbs.targetOS.contains("windows")
        cpp.dynamicLibraries: [
            "advapi32",
            "ole32",
            "shell32"
        ]
    }

    Group {
        name: "Analyzer"
        prefix: "analyzer/"
        files: [
            "analyzerutils.h",
            "detailederrorview.cpp",
            "detailederrorview.h",
        ]
    }

    QtcTestResources {
        prefix: "unit-tests/"
        files: ["**/*"]
    }

    Group {
        name: "images"
        files: "images/*.png"
        fileTags: "qt.core.resource_data"
    }

    Export {
        Depends { name: "CPlusPlus" }
    }
}
