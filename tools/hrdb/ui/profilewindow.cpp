#include "profilewindow.h"

#include <iostream>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QSettings>
#include <QDebug>
#include <QPushButton>

#include "../transport/dispatcher.h"
#include "../models/targetmodel.h"
#include "../models/session.h"
#include "quicklayout.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ProfileWindow::ProfileWindow(QWidget *parent, Session* pSession) :
    QDockWidget(parent),
    m_pSession(pSession),
    m_pTargetModel(pSession->m_pTargetModel),
    m_pDispatcher(pSession->m_pDispatcher)
{
    this->setWindowTitle("Profile");
    setObjectName("Console");

    // Layouts
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    QHBoxLayout* pTopLayout = new QHBoxLayout;
    auto pMainRegion = new QWidget(this);   // whole panel
    auto pTopRegion = new QWidget(this);      // top buttons/edits

    m_pStartStopButton = new QPushButton("Start", this);
    m_pResetButton = new QPushButton("Reset", this);

    pTopLayout->addWidget(m_pStartStopButton);
    pTopLayout->addWidget(m_pResetButton);

    SetMargins(pTopLayout);
    SetMargins(pMainLayout);

    pTopRegion->setLayout(pTopLayout);
    pMainRegion->setLayout(pMainLayout);
    setWidget(pMainRegion);

    loadSettings();

    connect(m_pTargetModel,     &TargetModel::connectChangedSignal,   this, &ProfileWindow::connectChangedSlot);
    connect(m_pTargetModel,     &TargetModel::startStopChangedSignal, this, &ProfileWindow::startStopChangedSlot);
    connect(m_pTargetModel,     &TargetModel::profileChangedSignal,   this, &ProfileWindow::profileChangedSlot);
    connect(m_pSession,         &Session::settingsChanged,            this, &ProfileWindow::settingsChangedSlot);

    connect(m_pStartStopButton, &QAbstractButton::clicked,            this, &ProfileWindow::startStopClicked);
    connect(m_pResetButton,     &QAbstractButton::clicked,            this, &ProfileWindow::resetClicked);

    // Refresh enable state
    connectChangedSlot();

    // Refresh font
    settingsChangedSlot();
}

ProfileWindow::~ProfileWindow()
{
}

void ProfileWindow::keyFocus()
{
    activateWindow();
}

void ProfileWindow::loadSettings()
{
    QSettings settings;
    settings.beginGroup("Profile");

    restoreGeometry(settings.value("geometry").toByteArray());
    settings.endGroup();
}

void ProfileWindow::saveSettings()
{
    QSettings settings;
    settings.beginGroup("Profile");

    settings.setValue("geometry", saveGeometry());
    settings.endGroup();
}

void ProfileWindow::connectChangedSlot()
{
    bool enable = m_pTargetModel->IsConnected() && !m_pTargetModel->IsRunning();
    m_pStartStopButton->setEnabled(enable);
    m_pResetButton->setEnabled(enable);
}

void ProfileWindow::startStopChangedSlot()
{
    bool enable = m_pTargetModel->IsConnected() && !m_pTargetModel->IsRunning();
    m_pStartStopButton->setEnabled(enable);
    m_pResetButton->setEnabled(enable);
}

void ProfileWindow::profileChangedSlot()
{
    if (m_pTargetModel->IsProfileEnabled())
        m_pStartStopButton->setText("Stop");
    else {
        m_pStartStopButton->setText("Start");
    }
}

void ProfileWindow::settingsChangedSlot()
{
    //    m_pTextArea->setFont(m_pSession->GetSettings().m_font);
}

void ProfileWindow::startStopClicked()
{
    if (!m_pTargetModel->IsConnected())
        return;

    if (m_pTargetModel->IsProfileEnabled())
        m_pDispatcher->SendCommandPacket("profile 0");
    else
        m_pDispatcher->SendCommandPacket("profile 1");
}

void ProfileWindow::resetClicked()
{
    m_pTargetModel->ProfileReset();
}
