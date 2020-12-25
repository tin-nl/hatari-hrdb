#ifndef MEMORYVIEWWIDGET_H
#define MEMORYVIEWWIDGET_H

#include <QDockWidget>
#include <QTableView>
#include "memory.h"

class TargetModel;
class Dispatcher;
class QComboBox;
class QCheckBox;

class MemoryViewTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column
    {
        kColAddress,
        kColData,
        kColAscii,
        kColCount
    };

    enum Mode
    {
        kModeByte,
        kModeWord,
        kModeLong
    };

    MemoryViewTableModel(QObject * parent, TargetModel* pTargetModel, Dispatcher* pDispatcher, int windowIndex);

    uint32_t GetRowCount() const { return m_rowCount; }
    Mode GetMode() const { return m_mode; }

    // returns false if expression is invalid
    bool SetAddress(std::string expression);
    void SetRowCount(uint32_t rowCount);
    void SetLock(bool locked);
    void SetMode(Mode mode);

    void MoveUp();
    void MoveDown();
    void PageUp();
    void PageDown();

    // "When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data()."
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    // "The model emits signals to indicate changes. For example, dataChanged() is emitted whenever items of data made available by the model are changed"
    // So I expect we can emit that if we see the target has changed

public slots:
    void memoryChangedSlot(int memorySlot, uint64_t commandId);
    void startStopChangedSlot();
    void connectChangedSlot();

private:
    void SetAddress(uint32_t address);
    void RequestMemory();
    void RecalcText();

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;

    // These are taken at the same time. Is there a race condition...?
    struct Row
    {
        QString m_hexText;
        QString m_asciiText;
    };

    std::vector<Row> m_rows;

    std::string m_addressExpression;
    bool    m_isLocked;
    uint32_t m_address;

    uint32_t m_bytesPerRow;
    Mode     m_mode;

    uint32_t m_rowCount;
    uint64_t m_requestId;
    int      m_windowIndex;        // e.g. "memory 0", "memory 1"
    MemorySlot  m_memSlot;
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
    MemoryViewWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* m_pDispatcher, int windowIndex);

public slots:
    void textEditChangedSlot();
    void lockChangedSlot();
    void modeComboBoxChanged(int index);

private:
    QLineEdit*           m_pLineEdit;
    QComboBox*           m_pComboBox;
    QCheckBox*           m_pLockCheckBox;
    MemoryTableView*     m_pTableView;

    MemoryViewTableModel* pModel;
    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
    QAbstractItemModel* m_pSymbolTableModel;
};

#endif // MEMORYVIEWWIDGET_H
