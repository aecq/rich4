#include "core/utils/audio_player.h"
#include <QBuffer>

AudioPlayer::AudioPlayer(QObject *parent) : QObject(parent)
{
    m_audioOutput = new QAudioOutput(this);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOutput);
    m_buffer = new QBuffer(this);
}

AudioPlayer &AudioPlayer::instance()
{
    static AudioPlayer* instance = new AudioPlayer();
    return *instance;
}

void AudioPlayer::play(QByteArray wavData)
{
    stop();
    m_buffer = new QBuffer(this);
    m_buffer->setData(wavData);
    m_buffer->open(QIODevice::ReadOnly);
    m_player->setSourceDevice(m_buffer);
    m_player->play();
}

void AudioPlayer::stop()
{
    m_player->stop();
    m_player->setSourceDevice(nullptr);
    if (m_buffer) {
        m_buffer->deleteLater();
        m_buffer = nullptr;
    }
}

void AudioPlayer::setVolume(qreal vol)
{
    m_audioOutput->setVolume(vol);
}