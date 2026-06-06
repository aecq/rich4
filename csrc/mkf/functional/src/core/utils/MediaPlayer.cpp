#include "core/utils/MediaPlayer.h"
#include <QBuffer>

MediaPlayer::MediaPlayer(QObject *parent) : QObject(parent)
{
    m_audioOutput = new QAudioOutput(this);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOutput);
    m_buffer = new QBuffer(this);
}

MediaPlayer &MediaPlayer::instance()
{
    static MediaPlayer* instance = new MediaPlayer();
    return *instance;
}

void MediaPlayer::play(QByteArray wavData)
{
    stop();
    if (m_buffer->isOpen()) {
        m_buffer->close();
    }
    m_buffer->setData(wavData);
    m_buffer->open(QIODevice::ReadOnly);
    m_player->setSourceDevice(m_buffer);
    m_player->play();
}

void MediaPlayer::stop()
{
    m_player->stop();
}

void MediaPlayer::setVolume(qreal vol)
{
    m_audioOutput->setVolume(vol);
}