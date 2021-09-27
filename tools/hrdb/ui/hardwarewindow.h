#ifndef HARDWAREWINDOW_H
#define HARDWAREWINDOW_H

#include <QDockWidget>
#include <QTableView>
#include <QFile>
#include "../models/memory.h"

class TargetModel;
class Dispatcher;
class Session;

class HardwareTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column
    {
        kColName,
        kColData,
        kColCount
    };

    HardwareTableModel(QObject * parent, TargetModel* pTargetModel, Dispatcher* pDispatcher);

    // "When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data()."
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
    void startStopChangedSlot();
    void memoryChangedSlot(int memorySlot, uint64_t commandId);

private:
    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
};


class HardwareWindow : public QDockWidget
{
    Q_OBJECT
public:
    HardwareWindow(QWidget *parent, Session* pSession);
    virtual ~HardwareWindow();

    // Grab focus and point to the main widget
    void keyFocus();

    void loadSettings();
    void saveSettings();

private slots:
    void connectChangedSlot();
    void settingsChangedSlot();

private:

    QTableView*         m_pTableView;

    Session*            m_pSession;
    TargetModel*        m_pTargetModel;
    Dispatcher*         m_pDispatcher;
};

#endif // HARDWAREWINDOW_H
