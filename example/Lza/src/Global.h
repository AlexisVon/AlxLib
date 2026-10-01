// Copyright (c) 2026 AlexisVon

#pragma once

#include <QDebug>
#include <QDirIterator>
#include <QFileDialog>
#include <QFrame>
#include <QStandardPaths>
#include <QString>
#include <QTime>

inline QString size2string(quint64 _size) {
    static const char* units[]{"B", "K", "M", "G", "T", "P"};
    int uidx{0};
    double sidx{(double) _size};
    while (sidx >= 1024.0 && uidx < 5) {
        sidx /= 1024.0;
        uidx++;
    }
    if (uidx == 0) return QString("%1 B").arg(_size);
    else return QString("%1 %2").arg(sidx, 0, 'f', 2).arg(units[uidx]);
}

inline QString time2string(quint64 _v) {
    static constexpr int min = 60;
    static constexpr int hour = 60 * 60;
    static constexpr int day = 60 * 60 * 24;
    if (_v < 1000) return QString("%1ms").arg(_v);
    _v /= 1000;
    if (_v < min) return QString("%1s").arg(_v);
    else if (_v < hour) return QString("%1m%2s").arg(_v / min).arg(_v % min);
    else if (_v < day) return QString("%1h%2m%3s").arg(_v / hour).arg(_v % hour / min).arg(_v % min);
    else return QString("%1d%2h%3m%4s").arg(_v / day).arg(_v % day / hour).arg(_v % hour / min).arg(_v % min);
};

inline QFrame* new_separator(QWidget* _parent, bool _h) {
    QFrame* result = new QFrame(_parent);
    result->setFrameShape(_h ? QFrame::HLine : QFrame::VLine);
    result->setFrameShadow(QFrame::Sunken);
    return result;
}

class LZAWidget : public QWidget {
    Q_OBJECT

public:
    LZAWidget(QWidget* _parent = nullptr) : QWidget(_parent) {}
    virtual ~LZAWidget() {}

public:
    inline void set_path(const QString& _path) { m_fpath = _path; }
signals:
    void sig_lock_window(bool _lock);
    void sig_show_message(const QString& _msg);
    void sig_show_speed(double _speed_ms);
    void sig_show_nedtime(quint64 _time_ms);
    void sig_show_srcsize(quint64 _size_byte);
    void sig_show_lzasize(quint64 _size_byte);

protected:
    QString m_fpath;
};
