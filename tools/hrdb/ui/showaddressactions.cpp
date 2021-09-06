#include "showaddressactions.h"

#include "../models/session.h"

ShowAddressActions::ShowAddressActions(Session* pSession) :
    m_activeAddress(0),
    m_pSession(pSession)
{
    for (int i = 0; i < kNumDisasmViews; ++i)
        m_pDisasmWindowActions[i] = new QAction(QString::asprintf("Show in Disassembly %d", i + 1), this);

    for (int i = 0; i < kNumMemoryViews; ++i)
        m_pMemoryWindowActions[i] = new QAction(QString::asprintf("Show in Memory %d", i + 1), this);

    m_pGraphicsInspectorAction = new QAction("Show in Graphics Inspector", this);

    for (int i = 0; i < kNumDisasmViews; ++i)
        connect(m_pDisasmWindowActions[i], &QAction::triggered, this, [=] () { this->disasmViewTrigger(i); } );

    for (int i = 0; i < kNumMemoryViews; ++i)
        connect(m_pMemoryWindowActions[i], &QAction::triggered, this, [=] () { this->memoryViewTrigger(i); } );

    connect(m_pGraphicsInspectorAction, &QAction::triggered, this, [=] () { this->graphicsInspectorTrigger(); } );
}

ShowAddressActions::~ShowAddressActions()
{

}

void ShowAddressActions::addActionsToMenu(QMenu* pMenu) const
{
    for (int i = 0; i < kNumDisasmViews; ++i)
        pMenu->addAction(m_pDisasmWindowActions[i]);

    for (int i = 0; i < kNumMemoryViews; ++i)
        pMenu->addAction(m_pMemoryWindowActions[i]);

    pMenu->addAction(m_pGraphicsInspectorAction);
}

void ShowAddressActions::setAddress(uint32_t address)
{
    m_activeAddress = address;
}

void ShowAddressActions::disasmViewTrigger(int windowIndex)
{
    emit m_pSession->addressRequested(Session::kDisasmWindow, windowIndex, m_activeAddress);
}

void ShowAddressActions::memoryViewTrigger(int windowIndex)
{
    emit m_pSession->addressRequested(Session::kMemoryWindow, windowIndex, m_activeAddress);
}

void ShowAddressActions::graphicsInspectorTrigger()
{
    emit m_pSession->addressRequested(Session::kGraphicsInspector, 0, m_activeAddress);
}
