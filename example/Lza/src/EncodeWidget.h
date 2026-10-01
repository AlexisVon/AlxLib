// Copyright (c) 2026 AlexisVon

#pragma once

#include "Global.h"
#include "ui_EncodeWidget.h"

#include <future>

class EncodeWidget : public LZAWidget {
    Q_OBJECT

public:
    EncodeWidget(QWidget* parent = nullptr);
    ~EncodeWidget();

private:
    void init_ui();
    void init_signal();
    void update_message();

    void lock_window(bool _lock);

    void add_path_item(const QString& _path);
    void exec_impl(QList<QPair<QString, QString>> _path_list, quint64 _total_size);

signals:
    void sig_show_execpb(int _val);
    void sig_show_execlog(const QString& _log);

public slots:
    void on_btn_exec_clicked();
    void on_btn_addfile_clicked();
    void on_btn_addfolder_clicked();
    void on_btn_clear_clicked();
    void on_cb_encrypt_toggled(bool _checked);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    Ui::EncodeWidget ui;
    std::future<void> m_task;
    bool m_abort{false};
    bool m_runing{false};
};
