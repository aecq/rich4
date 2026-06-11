#pragma once

#include <QBuffer>
#include <QMediaPlayer>
#include <QAudioOutput>

class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    static AudioPlayer& instance();

    void play(QByteArray wavData);
    void stop();
    void setVolume(qreal vol);

private:
    explicit AudioPlayer(QObject *parent = nullptr);

    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;
    QBuffer* m_buffer;
};