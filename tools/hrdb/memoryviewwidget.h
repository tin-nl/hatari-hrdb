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
    MemoryViewTableModel(QObject * parent, TargetModel* pTargetModel, Dispatcher* pDispatcher);

    uint32_t GetRowCount() const { return m_rowCount; }
    void SetAddress(uint32_t address);
    void SetRowCount(uint32_t rowCount);
    void MoveUp();
    void MoveDown();
    void PageUp();
    void PageDown();

    // "When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data()."
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    // "The model emits signals to indicate changes. For example, dataChanged() is emitted whenever items of data made available by the model are changed"
    // So I expect we can emit that if we see the target has changed

public slots:
    void memoryChangedSlot(int memorySlot, uint64_t commandId);
    void startStopChangedSlot();

private:
    void RequestMemory();

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;

    // These are taken at the same time. Is there a race condition...?
    std::vector<QString> m_rows;
    uint32_t m_address;
    uint32_t m_rowCount;
    uint64_t m_requestId;
};

class MemoryTableView : public QTableView
{
    Q_OBJECT
public:
    MemoryTableView(QWidget* parent, MemoryViewTableModel* pModel, TargetModel* pTargetModel);

public slots:

protected:
    QModelIndex moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers);
private:
    // override -- this doesn't trigger at the start?
    virtual void resizeEvent(QResizeEvent*);
private slots:
    void RecalcRowCount();

private:
    MemoryViewTableModel*     m_pTableModel;

    // Remembers which row we right-clicked on
    int                   m_rightClickRow;
};

class MemoryViewWidget : public QDockWidget
{
    Q_OBJECT
public:
    MemoryViewWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* m_pDispatcher);

public slots:
    void textEditChangedSlot();
private:
    QLineEdit*           m_pLineEdit;
    MemoryTableView*     m_pTableView;

    MemoryViewTableModel* pModel;
    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
};

#endif // MEMORYVIEWWIDGET_H
