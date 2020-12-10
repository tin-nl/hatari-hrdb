#ifndef MEMORYVIEWWIDGET_H
#define MEMORYVIEWWIDGET_H

#include <QDockWidget>
#include <QTableView>
#include "disassembler.h"

class TargetModel;
class Dispatcher;

class MemoryViewTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    MemoryViewTableModel(QObject * parent, TargetModel* pTargetModel);

    // "When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data()."
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    // "The model emits signals to indicate changes. For example, dataChanged() is emitted whenever items of data made available by the model are changed"
    // So I expect we can emit that if we see the target has changed

public slots:
    void memoryChangedSlot(int memorySlot, uint64_t commandId);

private:
    TargetModel* m_pTargetModel;

    // These are taken at the same time. Is there a race condition...?
    std::vector<QString> m_rows;
    uint32_t m_address;
};

class MemoryViewWidget : public QDockWidget
{
    Q_OBJECT
public:
    MemoryViewWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* m_pDispatcher);

public slots:
    void startStopChangedSlot();
    void textEditChangedSlot();
private:
    QLineEdit*      m_pLineEdit;
    QTableView*     m_pTableView;

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
};

#endif // MEMORYVIEWWIDGET_H
