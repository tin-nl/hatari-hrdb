#ifndef BREAKPOINTSWIDGET_H
#define BREAKPOINTSWIDGET_H

#include <QDockWidget>
#include <QTableView>

class TargetModel;
class Dispatcher;
class QComboBox;
class QCheckBox;

class BreakpointsTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column
    {
        kColId,
        kColExpression,
        kColConditionCount,
        kColHitCount,
        kColCount
    };

    BreakpointsTableModel(QObject * parent, TargetModel* pTargetModel, Dispatcher* pDispatcher);

    // "When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data()."
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    // "The model emits signals to indicate changes. For example, dataChanged() is emitted whenever items of data made available by the model are changed"
    // So I expect we can emit that if we see the target has changed

public slots:
    void breakpointsChangedSlot();

private:

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
};

class BreakpointsTableView : public QTableView
{
    Q_OBJECT
public:
    BreakpointsTableView(QWidget* parent, BreakpointsTableModel* pModel, TargetModel* pTargetModel);

public slots:

protected:

private:

private slots:

private:
    BreakpointsTableModel*     m_pTableModel;

    // Remembers which row we right-clicked on
    int                   m_rightClickRow;
};

class BreakpointsWidget : public QDockWidget
{
    Q_OBJECT
public:
    BreakpointsWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* m_pDispatcher);

public slots:

private:
    BreakpointsTableView*     m_pTableView;
    BreakpointsTableModel* pModel;
    TargetModel*        m_pTargetModel;
    Dispatcher*         m_pDispatcher;
    QAbstractItemModel* m_pSymbolTableModel;
};

#endif // BREAKPOINTSWIDGET_H
