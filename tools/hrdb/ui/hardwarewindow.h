#ifndef HARDWAREWINDOW_H
#define HARDWAREWINDOW_H

#include <QDockWidget>
#include <QAbstractItemModel>
#include <QLabel>
#include "../models/memory.h"

class QTreeView;
class QFormLayout;
class QHBoxLayout;
class QLabel;

class TargetModel;
class Dispatcher;
class Session;

namespace Regs
{
    struct FieldDef;
}

class HardwareField;

class ExpandLabel : public QLabel
{
    Q_OBJECT
public:
    ExpandLabel(QWidget* parent);
signals:
    void clicked();

protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
private:
};

class Expander : public QWidget
{
    Q_OBJECT
public:
    // Two widgets, the clickable item
    QWidget*                m_pTop;
    QWidget*                m_pBottom;
    QHBoxLayout*            m_pTopLayout;
    QFormLayout*            m_pBottomLayout;

    ExpandLabel*            m_pButton;

    // Then the form-layout with child items
    Expander(QWidget* parent, QString text);

public slots:
    void buttonPressedSlot();

private:
    void UpdateState();

    QString m_text;
    bool m_expanded;
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

public slots:
    void connectChangedSlot();
    void startStopChangedSlot();
    void memoryChangedSlot(int memorySlot, uint64_t commandId);
    void ymChangedSlot();
    void settingsChangedSlot();

private:
    void addField(QFormLayout* pLayout, const QString& title, const Regs::FieldDef& def);
    void addMultiField(QFormLayout *pLayout, const QString &title, const Regs::FieldDef** defs);
    void addShared(QFormLayout *pLayout, const QString &title, HardwareField* pField);

    Session*                    m_pSession;
    TargetModel*                m_pTargetModel;
    Dispatcher*                 m_pDispatcher;

    Memory                      m_videoMem;
    Memory                      m_mfpMem;

    QVector<HardwareField*>     m_fields;
};

#endif // HARDWAREWINDOW_H
