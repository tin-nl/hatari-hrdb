#ifndef PROFILEWINDOW_H
#define PROFILEWINDOW_H

#include <QDockWidget>
#include <QTableView>

class TargetModel;
class Dispatcher;
class Session;
class QLabel;
class QPushButton;
class QTextEdit;

class ProfileTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:

    struct Entry
    {
        QModelIndex     index;
        QString         address;
        uint32_t        instructionCount;
        unsigned long   cycleCount;
    };

    enum Column
    {
        kColAddress,
        kColInstructionCount,
        kColCycles,
        kColCount
    };

    ProfileTableModel(QObject * parent, TargetModel* pTargetModel, Dispatcher* pDispatcher);

    // "When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data()."
    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

public slots:

    // NO CHECK add symbol change slot
    void profileChangedSlot();
    void symbolChangedSlot();

private:

    void updateRows();

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;

    QMap<uint32_t, Entry>   map;
    QVector<Entry>          entries;
};

class ProfileTableView : public QTableView
{
    Q_OBJECT
public:
    ProfileTableView(QWidget* parent, ProfileTableModel* pModel);
    virtual ~ProfileTableView() override;

public slots:

protected:

private:

private slots:

private:
    ProfileTableModel*      m_pTableModel;

    // Remembers which row we right-clicked on
    int                     m_rightClickRow;

};

class ProfileWindow : public QDockWidget
{
    Q_OBJECT
public:
    ProfileWindow(QWidget *parent, Session* pSession);
    virtual ~ProfileWindow() override;

    // Grab focus and point to the main widget
    void keyFocus();

    void loadSettings();
    void saveSettings();

private slots:
    void connectChangedSlot();
    void startStopChangedSlot();
    void profileChangedSlot();
    void settingsChangedSlot();

    void startStopClicked();
    void resetClicked();

private:
    void updateText();

    Session*            m_pSession;
    TargetModel*        m_pTargetModel;
    Dispatcher*         m_pDispatcher;

    QPushButton*        m_pStartStopButton;
    QPushButton*        m_pResetButton;

    ProfileTableView*   m_pTableView;
    ProfileTableModel*  m_pTableModel;
};

#endif // PROFILEWINDOW_H
