// Copyright (c) 2026 AlexisVon

#pragma once

#include "Global.h"
#include "ui_Alexis_Lza.h"

#include <QLabel>
#include <QMouseEvent>

class AlexisLza : public QMainWindow {
    Q_OBJECT

public:
    AlexisLza(QWidget* parent = nullptr);
    ~AlexisLza();

private:
    void init_ui();
    void init_signal();
    void init_argv();

    void lock_window(bool _lock);
    void show_message(const QString& _msg);
    void show_speed(double _speed_ms);
    void show_nedtime(quint64 _time_ms);
    void show_srcsize(quint64 _size_byte);
    void show_lzasize(quint64 _size_byte);

signals:
    void sig_lock_window(bool _lock);
    void sig_show_message(const QString& _msg);

public slots:
    void on_btn_path_clicked();
    void on_le_path_textChanged(const QString& _path);

protected:
    virtual void mouseMoveEvent(QMouseEvent* _ev) override;
    virtual void mousePressEvent(QMouseEvent* _ev) override;
    virtual void mouseReleaseEvent(QMouseEvent* _ev) override;

private:
    Ui::AlexisLza ui;
    QLabel* ui_lb_speed{nullptr};
    QLabel* ui_lb_srcsize{nullptr};
    QLabel* ui_lb_lzasize{nullptr};
    QLabel* ui_lb_nedtime{nullptr};

    QPoint ui_mouse_point;
    QRect ui_maxdrag_rect;
    bool ui_mouse_press{false};
    bool ui_maxdrag_flag{false};
};
