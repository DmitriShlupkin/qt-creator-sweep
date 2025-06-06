// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "perfprofilerstatisticsmodel.h"
#include "perfprofilerstatisticsview.h"
#include "perfprofilertracemanager.h"

#include <coreplugin/minisplitter.h>

#include <utils/basetreeview.h>

#include <QApplication>
#include <QClipboard>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace PerfProfiler::Internal {

class StatisticsView : public Utils::BaseTreeView
{
public:
    StatisticsView(QWidget *parent);
    void clear();
    QString rowToString(int row) const;

    void copyTableToClipboard() const;
    void copySelectionToClipboard() const;
};

class HexNumberDelegate : public QStyledItemDelegate
{
public:
    HexNumberDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QString displayText(const QVariant &value, const QLocale &locale) const final
    {
        Q_UNUSED(locale)
        return QString::fromLatin1("0x%1").arg(value.toULongLong(), 16, 16, QLatin1Char('0'));
    }
};

PerfProfilerStatisticsView::PerfProfilerStatisticsView()
{
    setObjectName(QLatin1String("PerfProfilerStatisticsView"));

    m_mainView = new StatisticsView(this);
    m_parentsView = new StatisticsView(this);
    m_childrenView = new StatisticsView(this);

    // widget arrangement
    QVBoxLayout *groupLayout = new QVBoxLayout;
    groupLayout->setContentsMargins(0,0,0,0);
    groupLayout->setSpacing(0);

    Core::MiniSplitter *splitterVertical = new Core::MiniSplitter;
    splitterVertical->addWidget(m_mainView);
    Core::MiniSplitter *splitterHorizontal = new Core::MiniSplitter;
    splitterHorizontal->addWidget(m_parentsView);
    splitterHorizontal->addWidget(m_childrenView);
    splitterHorizontal->setOrientation(Qt::Horizontal);
    splitterVertical->addWidget(splitterHorizontal);
    splitterVertical->setOrientation(Qt::Vertical);
    splitterVertical->setStretchFactor(0,5);
    splitterVertical->setStretchFactor(1,2);
    groupLayout->addWidget(splitterVertical);
    setLayout(groupLayout);

    auto mainModel = new PerfProfilerStatisticsMainModel(this);

    PerfProfilerStatisticsRelativesModel *children = mainModel->children();
    PerfProfilerStatisticsRelativesModel *parents = mainModel->parents();

    m_mainView->setModel(mainModel);
    m_childrenView->setModel(children);
    m_parentsView->setModel(parents);

    auto propagateSelection = [this, children, parents](int locationId) {
        children->selectByTypeId(locationId);
        parents->selectByTypeId(locationId);
        const PerfEventType::Location &location = traceManager().location(locationId);
        const QByteArray &file = traceManager().string(location.file);
        if (!file.isEmpty())
            emit gotoSourceLocation(QString::fromUtf8(file), location.line, location.column);
        emit typeSelected(locationId);
    };

    connect(m_mainView, &QAbstractItemView::activated, this,
            [propagateSelection, mainModel](QModelIndex index) {
        propagateSelection(mainModel->typeId(index.row()));
    });

    connect(m_childrenView, &QAbstractItemView::activated, this,
            [this, propagateSelection, mainModel, children](QModelIndex index) {
        int typeId = children->typeId(index.row());
        m_mainView->setCurrentIndex(mainModel->index(mainModel->rowForTypeId(typeId), 0));
        propagateSelection(typeId);
    });

    connect(m_parentsView, &QAbstractItemView::activated, this,
            [this, propagateSelection, mainModel, parents](QModelIndex index) {
        int typeId = parents->typeId(index.row());
        m_mainView->setCurrentIndex(mainModel->index(mainModel->rowForTypeId(typeId), 0));
        propagateSelection(typeId);
    });
}

bool PerfProfilerStatisticsView::focusedTableHasValidSelection() const
{
    if (m_mainView->hasFocus())
        return m_mainView->currentIndex().isValid();
    else if (m_childrenView->hasFocus())
        return m_childrenView->currentIndex().isValid();
    else if (m_parentsView->hasFocus())
        return m_parentsView->currentIndex().isValid();
    return false;
}

void PerfProfilerStatisticsView::selectByTypeId(int typeId)
{
    const PerfProfilerStatisticsMainModel *mainModel =
            static_cast<const PerfProfilerStatisticsMainModel *>(m_mainView->model());
    if (mainModel) {
        if (m_mainView->currentIndex().isValid() &&
                mainModel->typeId(m_mainView->currentIndex().row()) == typeId)
            return;

        m_mainView->setCurrentIndex(mainModel->index(mainModel->rowForTypeId(typeId), 0));
        mainModel->children()->selectByTypeId(typeId);
        mainModel->parents()->selectByTypeId(typeId);
    }
}

void PerfProfilerStatisticsView::copyFocusedTableToClipboard() const
{
    if (m_mainView->hasFocus())
        m_mainView->copyTableToClipboard();
    else if (m_childrenView->hasFocus())
        m_childrenView->copyTableToClipboard();
    else if (m_parentsView->hasFocus())
        m_parentsView->copyTableToClipboard();
}

void PerfProfilerStatisticsView::copyFocusedSelectionToClipboard() const
{
    if (m_mainView->hasFocus())
        m_mainView->copySelectionToClipboard();
    else if (m_childrenView->hasFocus())
        m_childrenView->copySelectionToClipboard();
    else if (m_parentsView->hasFocus())
        m_parentsView->copySelectionToClipboard();
}

StatisticsView::StatisticsView(QWidget *parent) :
    Utils::BaseTreeView(parent)
{
    setObjectName(QLatin1String("StatisticsView"));
    setSortingEnabled(true);
    setAlternatingRowColors(true);
    setItemDelegateForColumn(0, new HexNumberDelegate(this));
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void StatisticsView::clear()
{
    model()->removeRows(0, model()->rowCount());
}

QString StatisticsView::rowToString(int row) const
{
    QString str;
    const QAbstractItemModel *m = model();
    str += QString::fromLatin1("0x%1").arg(m->data(m->index(row, 0)).toULongLong(), 16, 16,
                                             QLatin1Char('0'));
    for (int column = 1; column < m->columnCount(); ++column)
        str += QLatin1Char('\t') + m->data(m->index(row, column)).toString();
    return str + QLatin1Char('\n');
}

static void sendToClipboard(const QString &str)
{
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard->supportsSelection())
        clipboard->setText(str, QClipboard::Selection);
    clipboard->setText(str, QClipboard::Clipboard);
}

void StatisticsView::copyTableToClipboard() const
{
    const QAbstractItemModel *m = model();
    QString str;
    // headers
    int columnCount = m->columnCount();
    for (int column = 0; column < columnCount; ++column) {
        str += m->headerData(column, Qt::Horizontal).toString();
        if (column < columnCount - 1)
            str += QLatin1Char('\t');
        else
            str += QLatin1Char('\n');
    }
    // data
    int rowCount = m->rowCount();
    for (int row = 0; row < rowCount; ++row)
        str += rowToString(row);

    sendToClipboard(str);
}

void StatisticsView::copySelectionToClipboard() const
{
    if (currentIndex().isValid())
        sendToClipboard(rowToString(currentIndex().row()));
}

} // namespace PerfProfiler::Internal
