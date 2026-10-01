// Copyright (c) 2026 AlexisVon

#include "Alexis_Lza.h"

#include <QFileDialog>
#include <QStandardPaths>

AlexisLza::AlexisLza(QWidget* parent)
    : QMainWindow(parent) {
    ui.setupUi(this);
    init_ui();
    init_signal();
    init_argv();
}

AlexisLza::~AlexisLza() {
}

void AlexisLza::init_ui() {
    setAcceptDrops(true);
    setWindowFlags(Qt::FramelessWindowHint);

    ui.status->addPermanentWidget(ui_lb_speed = new QLabel("SPD", ui.status));
    ui.status->addPermanentWidget(ui_lb_nedtime = new QLabel("NED", ui.status));
    ui.status->addPermanentWidget(new_separator(ui.status, false));
    ui.status->addPermanentWidget(ui_lb_lzasize = new QLabel("LZA", ui.status));
    ui.status->addPermanentWidget(new_separator(ui.status, false));
    ui.status->addPermanentWidget(ui_lb_srcsize = new QLabel("SRC", ui.status));
}

void AlexisLza::init_signal() {
    connect(ui.btn_window_exit, &QPushButton::clicked, this, [this]() {showMinimized(); QApplication::exit(0); });
    connect(ui.btn_window_max, &QPushButton::clicked, this, [&]() { isMaximized() || isFullScreen() ? showNormal() : showFullScreen(); });
    connect(ui.btn_window_min, &QPushButton::clicked, this, [&]() { showMinimized(); });
    connect(ui.btn_window_encode, &QPushButton::clicked, this, [&]() { ui.sw_main->setCurrentIndex(0); });
    connect(ui.btn_window_decode, &QPushButton::clicked, this, [&]() { ui.sw_main->setCurrentIndex(1); });

    connect(this, &AlexisLza::sig_lock_window, this, &AlexisLza::lock_window);
    connect(this, &AlexisLza::sig_show_message, this, &AlexisLza::show_message);

    auto connect_lza = [this](LZAWidget* _w) {
        connect(_w, &LZAWidget::sig_lock_window, this, &AlexisLza::lock_window);
        connect(_w, &LZAWidget::sig_show_message, this, &AlexisLza::show_message);
        connect(_w, &LZAWidget::sig_show_speed, this, &AlexisLza::show_speed);
        connect(_w, &LZAWidget::sig_show_nedtime, this, &AlexisLza::show_nedtime);
        connect(_w, &LZAWidget::sig_show_srcsize, this, &AlexisLza::show_srcsize);
        connect(_w, &LZAWidget::sig_show_lzasize, this, &AlexisLza::show_lzasize);
    };

    connect_lza(ui.encode);
    connect_lza(ui.decode);
}

void AlexisLza::init_argv() {
    QStringList args = QApplication::arguments();
    if (args.size() <= 1) return ui.sw_main->setCurrentIndex(0);
    ui.sw_main->setCurrentIndex(1);
    ui.le_path->setText(args.mid(1).join(' '));
}

void AlexisLza::lock_window(bool _lock) {
    _lock = !_lock;
    ui.btn_path->setEnabled(_lock);
    ui.le_path->setEnabled(_lock);
    ui.btn_window_encode->setEnabled(_lock);
    ui.btn_window_decode->setEnabled(_lock);
}

void AlexisLza::show_message(const QString& _msg) {
    ui.status->showMessage(_msg);
}

void AlexisLza::show_speed(double _speed_ms) {
    if (_speed_ms == 0.0) ui_lb_speed->setText("SPD");
    else ui_lb_speed->setText(QString("%1/s").arg(size2string(_speed_ms * 1000.0)));
}

void AlexisLza::show_nedtime(quint64 _time_ms) {
    if (_time_ms == 0) ui_lb_nedtime->setText("NED");
    else ui_lb_nedtime->setText(QString("%1").arg(time2string(_time_ms)));
}

void AlexisLza::show_srcsize(quint64 _size_byte) {
    if (_size_byte == 0) ui_lb_srcsize->setText("SRC");
    else ui_lb_srcsize->setText(QString("SRC: %1").arg(size2string(_size_byte)));
}

void AlexisLza::show_lzasize(quint64 _size_byte) {
    if (_size_byte == 0) ui_lb_lzasize->setText("LZA");
    else ui_lb_lzasize->setText(QString("LZA: %1").arg(size2string(_size_byte)));
}

void AlexisLza::on_btn_path_clicked() {
    if (ui.sw_main->currentIndex() == 0) {
        QString path =
            QFileDialog::getSaveFileName(this, u8"选择LZA文件输出路径，编辑名称",
                                         ui.le_path->text().isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) : ui.le_path->text(), "LZA(*.lza)");
        if (!path.isEmpty()) ui.le_path->setText(path);
    } else {
        QString path =
            QFileDialog::getOpenFileName(this, u8"选择要打开的LZA文件",
                                         ui.le_path->text().isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) : ui.le_path->text(), "LZA(*.lza)");
        if (!path.isEmpty()) ui.le_path->setText(path);
    }
}

void AlexisLza::on_le_path_textChanged(const QString& _path) {
    ui.encode->set_path(_path);
    ui.decode->set_path(_path);
}

void AlexisLza::mouseMoveEvent(QMouseEvent* _ev) {
    if (ui_mouse_press) {
        if (isMaximized() || isFullScreen()) {
            ui_maxdrag_flag = true;
            ui_maxdrag_rect = frameGeometry();
            ui_mouse_point = _ev->scenePosition().toPoint();
            showNormal();
        } else if (ui_maxdrag_flag) {
            ui_maxdrag_flag = false;
            QRect new_rect = frameGeometry();
            const qreal scale_w = (qreal) new_rect.width() / (qreal) ui_maxdrag_rect.width();
            const qreal scale_h = (qreal) new_rect.height() / (qreal) ui_maxdrag_rect.height();

            const QPoint scale_pos(ui_mouse_point.x() * scale_w, ui_mouse_point.y() * scale_h);
            const QPoint global_pos = mapToGlobal(scale_pos);

            move(this->pos() + _ev->globalPosition().toPoint() - global_pos);
            ui_mouse_point = _ev->globalPosition().toPoint() - this->pos();
        } else if (_ev->globalPosition().toPoint().y() == 0) {
            showMaximized();
            ui_mouse_press = false;
        } else move(_ev->globalPosition().toPoint() - ui_mouse_point);
    }
}

void AlexisLza::mousePressEvent(QMouseEvent* _ev) {
    if (_ev->button() == Qt::LeftButton && childAt(_ev->pos()) == ui.fm_head) {
        ui_mouse_press = true;
        ui_mouse_point = _ev->globalPosition().toPoint() - this->pos();
    }
}

void AlexisLza::mouseReleaseEvent(QMouseEvent* _ev) {
    ui_mouse_press = false;
}
