#ifndef NONANTIALIASIMAGE_H
#define NONANTIALIASIMAGE_H

#include <QWidget>

class Session;

// Taken from https://forum.qt.io/topic/94996/qlabel-and-image-antialiasing/5
class NonAntiAliasImage : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(NonAntiAliasImage)
public:
    NonAntiAliasImage(QWidget* parent, Session* pSession);
    virtual ~NonAntiAliasImage() override;

    void setPixmap(int width, int height);
    uint8_t* AllocBitmap(int size);
    void SetRunning(bool runFlag);

    QVector<QRgb>   m_colours;

    const QString& GetString() { return m_infoString; }
signals:
    void StringChanged();

protected:
    virtual void paintEvent(QPaintEvent*) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void settingsChangedSlot();

private:
    void UpdateString();

    Session*        m_pSession;
    QPixmap         m_pixmap;
    QPointF         m_mousePos;

    // Underlying bitmap data
    uint8_t*        m_pBitmap;
    int             m_bitmapSize;

    QString         m_infoString;
    bool            m_bRunningMask;
};


#endif // NONANTIALIASIMAGE_H
