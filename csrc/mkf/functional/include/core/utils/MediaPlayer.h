#pragma once

#include <QBuffer>
#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>

class MediaPlayer : public QObject
{
    Q_OBJECT
public:
    static MediaPlayer& instance();

    void play(QByteArray wavData);  // 播放 WAV 内存数据
    void stop();
    void setVolume(qreal vol);

private:
    explicit MediaPlayer(QObject *parent = nullptr);

    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;
    QBuffer* m_buffer;
};