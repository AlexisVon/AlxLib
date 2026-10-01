// Copyright (c) 2026 AlexisVon

#pragma once

#include "Global.h"
#include "ui_DecodeWidget.h"

#include <future>

class FSysTreeModel;
namespace alx {
    namespace fpacker {
        class decoder;
    }
}

class DecodeWidget : public LZAWidget {
    Q_OBJECT

public:
    DecodeWidget(QWidget* parent = nullptr);
    ~DecodeWidget();

private:
    void init_ui();
    void init_signal();
    void update_message();

    void lock_window(bool _lock);

    void export_data();
    void expand_item();
    void collapse_item();

signals:
    void sig_show_execpb(int _val);
    void sig_show_execlog(const QString& _log);

public slots:
    void on_btn_exec_clicked();
    void on_tv_fsys_customContextMenuRequested(const QPoint& _pos);

private:
    Ui::DecodeWidget ui;
    QMenu* ui_menu{nullptr};
    QAction* ui_act_export{nullptr};
    QAction* ui_act_expand{nullptr};
    QAction* ui_act_collapse{nullptr};
    QAction* ui_act_export_all{nullptr};
    QAction* ui_act_expand_all{nullptr};
    QAction* ui_act_collapse_all{nullptr};
    FSysTreeModel* ui_fsys{nullptr};
    QModelIndex ui_selected;

    std::future<void> m_task;
    alx::fpacker::decoder* m_decoder{nullptr};
    bool m_abort{false};
    bool m_runing{false};
};
