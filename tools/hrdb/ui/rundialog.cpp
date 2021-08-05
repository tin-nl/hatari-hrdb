#include "rundialog.h"
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QSettings>
#include <QCloseEvent>
#include <QFileDialog>
#include <QPushButton>

#include "../models/targetmodel.h"
#include "../transport/dispatcher.h"
#include "quicklayout.h"

RunDialog::RunDialog(QWidget *parent, TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDialog(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    this->setWindowTitle(tr("Run Hatari"));

    QPushButton* pOkButton = new QPushButton("&OK", this);
    pOkButton->setDefault(true);
    QPushButton* pCancelButton = new QPushButton("&Cancel", this);

    QHBoxLayout* pHLayout = new QHBoxLayout(this);
    pHLayout->addWidget(pOkButton);
    pHLayout->addWidget(pCancelButton);
    QWidget* pButtonContainer = new QWidget(this);
    pButtonContainer->setLayout(pHLayout);
    QPushButton* pExeButton = new QPushButton("Executable", this);
    QPushButton* pWDButton = new QPushButton("Working Directory", this);

    QWidget* list1, * list2, * list3;

    {
        m_pExecutableTextEdit = new QLineEdit("hatari", this);

        QWidget* list[] = { pExeButton, m_pExecutableTextEdit, nullptr};
        list1 = CreateHorizLayout(this, list);
    }

    {
        m_pArgsTextEdit = new QLineEdit("", this);
        QLabel* pFront = new QLabel("Arguments", this);

        QWidget* list[] = { pFront, m_pArgsTextEdit, nullptr };
        list2 = CreateHorizLayout(this, list);
    }

    {
        m_pWorkingDirectoryTextEdit = new QLineEdit("", this);

        QWidget* list[] = { pWDButton, m_pWorkingDirectoryTextEdit, nullptr };
        list3 = CreateHorizLayout(this, list);
    }

    QVBoxLayout* pLayout = new QVBoxLayout(this);
    pLayout->addWidget(list1);
    pLayout->addWidget(list2);
    pLayout->addWidget(list3);
    pLayout->addWidget(pButtonContainer);

    connect(pExeButton, &QPushButton::clicked, this, &RunDialog::exeClicked);
    connect(pWDButton, &QPushButton::clicked, this, &RunDialog::workingDirectoryClicked);
    connect(pOkButton, &QPushButton::clicked, this, &RunDialog::okClicked);
    connect(pOkButton, &QPushButton::clicked, this, &RunDialog::accept);
    connect(pCancelButton, &QPushButton::clicked, this, &RunDialog::reject);
    loadSettings();
    this->setLayout(pLayout);
}

RunDialog::~RunDialog()
{

}

void RunDialog::loadSettings()
{
    QSettings settings;
    settings.beginGroup("RunDialog");

    restoreGeometry(settings.value("geometry").toByteArray());
    m_pExecutableTextEdit->setText(settings.value("exe", QVariant("hatari")).toString());
    m_pArgsTextEdit->setText(settings.value("args", QVariant("")).toString());
    m_pWorkingDirectoryTextEdit->setText(settings.value("workingDirectory", QVariant("")).toString());

    settings.endGroup();
}

void RunDialog::saveSettings()
{
    QSettings settings;
    settings.beginGroup("RunDialog");

    settings.setValue("geometry", saveGeometry());
    settings.setValue("exe", m_pExecutableTextEdit->text());
    settings.setValue("args", m_pArgsTextEdit->text());
    settings.setValue("workingDirectory", m_pWorkingDirectoryTextEdit->text());
    settings.endGroup();
}

void RunDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
}

void RunDialog::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void RunDialog::okClicked()
{
    QProcess proc;
    QStringList args = m_pArgsTextEdit->text().split(" ");

    proc.setProgram(m_pExecutableTextEdit->text());
    proc.setArguments(args);
    proc.setWorkingDirectory(m_pWorkingDirectoryTextEdit->text());
    proc.startDetached();

    saveSettings();
}

void RunDialog::exeClicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
          tr("Choose Hatari executable"));
    if (filename.size() != 0)
        m_pExecutableTextEdit->setText(filename);
    saveSettings();
}

void RunDialog::workingDirectoryClicked()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::Directory);

    QStringList fileNames;
    if (dialog.exec())
    {
        fileNames = dialog.selectedFiles();
        if (fileNames.length() > 0)
            m_pWorkingDirectoryTextEdit->setText(fileNames[0]);
        saveSettings();
    }
}

