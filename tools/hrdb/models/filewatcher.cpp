#include "filewatcher.h"
#include <QString>
#include <QFileSystemWatcher>
#include <QObject>
#include "transport/dispatcher.h"
#include "models/targetmodel.h"
#include "session.h"

FileWatcher::FileWatcher(const Session* pSession):m_pSession(pSession)
{
    m_pFileSystemWatcher=new QFileSystemWatcher();
    QObject::connect( m_pFileSystemWatcher, &QFileSystemWatcher::fileChanged, this, &FileWatcher::handleFileChanged );
}

FileWatcher::~FileWatcher()
{
    delete m_pFileSystemWatcher;
}

void FileWatcher::clear()
{
    m_pFileSystemWatcher->removePaths(m_pFileSystemWatcher->directories());
}

void FileWatcher::handleFileChanged(QString path)
{
    //@FIXME:nope.
    ((Session*)m_pSession)->resetEmulator();
}
