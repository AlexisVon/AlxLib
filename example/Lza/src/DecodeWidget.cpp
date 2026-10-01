// Copyright (c) 2026 AlexisVon

#include "DecodeWidget.h"
#include "FSysTree.h"

#include <QMenu>

#include <afpacker.h>
#include <astream_ex.h>

DecodeWidget::DecodeWidget(QWidget* parent)
    : LZAWidget(parent) {
    ui.setupUi(this);
    init_ui();
    init_signal();
}

DecodeWidget::~DecodeWidget() {
    m_abort = true;
    if (m_task.valid()) m_task.get();
    delete m_decoder;
}

void DecodeWidget::init_ui() {
    ui_menu = new QMenu(this);
    ui_act_export = new QAction(u8"导出", ui_menu);
    ui_act_expand = new QAction(u8"展开", ui_menu);
    ui_act_collapse = new QAction(u8"折叠", ui_menu);
    ui_act_export_all = new QAction(u8"导出全部", ui_menu);
    ui_act_expand_all = new QAction(u8"展开全部", ui_menu);
    ui_act_collapse_all = new QAction(u8"折叠全部", ui_menu);
    ui_menu->addAction(ui_act_export);
    ui_menu->addAction(ui_act_export_all);
    ui_menu->addSeparator();
    ui_menu->addAction(ui_act_expand);
    ui_menu->addAction(ui_act_collapse);
    ui_menu->addAction(ui_act_expand_all);
    ui_menu->addAction(ui_act_collapse_all);

    ui.tv_fsys->setContextMenuPolicy(Qt::CustomContextMenu);
    ui.tv_fsys->setSelectionMode(QAbstractItemView::SingleSelection);
    ui.tv_fsys->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tv_fsys->setSortingEnabled(true);
    ui_fsys = new FSysTreeModel(ui.tv_fsys);
    ui.tv_fsys->setModel(ui_fsys);
    ui.tv_fsys->header()->setStretchLastSection(false);
    ui.tv_fsys->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui.tv_fsys->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui.tv_fsys->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui.tv_fsys->header()->setMinimumSectionSize(40);
    ui.te_exec_log->setVisible(false);
}

void DecodeWidget::init_signal() {
    connect(this, &DecodeWidget::sig_lock_window, this, &DecodeWidget::lock_window);
    connect(this, &DecodeWidget::sig_show_execpb, this, [this](int _val) { ui.pb_exec->setValue(_val); });
    connect(this, &DecodeWidget::sig_show_execlog, this, [this](const QString& _log) {ui.te_exec_log->setVisible(true); ui.te_exec_log->append(_log); });
    connect(ui_act_export, &QAction::triggered, this, &DecodeWidget::export_data);
    connect(ui_act_export_all, &QAction::triggered, this, [this]() {ui_selected = QModelIndex(); export_data(); });
    connect(ui_act_expand, &QAction::triggered, this, &DecodeWidget::expand_item);
    connect(ui_act_expand_all, &QAction::triggered, this, [this]() {ui_selected = QModelIndex(); expand_item(); });
    connect(ui_act_collapse, &QAction::triggered, this, &DecodeWidget::collapse_item);
    connect(ui_act_collapse_all, &QAction::triggered, this, [this]() {ui_selected = QModelIndex(); collapse_item(); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(ui.cb_password, &QCheckBox::checkStateChanged, this,
            [this](Qt::CheckState _s) { ui.le_password->setEchoMode(_s == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password); });
#else
    connect(ui.cb_password, &QCheckBox::stateChanged, this,
            [this](int _s) { ui.le_password->setEchoMode(_s == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password); });
#endif
    connect(ui.le_password, &QLineEdit::returnPressed, this, &DecodeWidget::on_btn_exec_clicked);
}

void DecodeWidget::update_message() {
    if (nullptr == m_decoder) {
        emit sig_show_lzasize(0);
        emit sig_show_srcsize(0);
        emit sig_show_speed(0.0);
    } else {
        emit sig_show_lzasize(m_decoder->get_stream()->total());
        emit sig_show_srcsize(ui_fsys->root()->size());
        emit sig_show_speed(0.0);
    }
    ui.te_exec_log->clear();
    ui.te_exec_log->setVisible(false);
}

void DecodeWidget::lock_window(bool _lock) {
    _lock = !_lock;
    ui.fm_password->setEnabled(_lock);
}

void DecodeWidget::export_data() {
    if (nullptr == m_decoder) return;

    static QString last_path;
    QString output = QFileDialog::getExistingDirectory(this, u8"选择要添加的文件夹",
                                                       last_path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) : last_path);
    if (output.isEmpty()) return;

    std::string root;
    std::list<std::string> path;
    QString url;
    quint64 size{0};
    if (ui_selected.isValid()) {
        FSysTreeItem* item = (FSysTreeItem*) ui_selected.internalPointer();
        root = item->getRoot();
        path = item->getPath();
        size = item->size();
        url = FSysTreeItem::CalUrl(root, path);
    } else {
        root = "*";
        path.clear();
        url = "*";
        size = ui_fsys->root()->size();
    }

    emit sig_lock_window(true);
    emit sig_show_execpb(0);
    emit update_message();
    emit sig_show_message(QString(u8"数据\"%1\"正在导出到\"%2\"").arg(url).arg(output));

    ui.btn_exec->setText(u8"中断执行");
    m_abort = false;
    m_runing = true;
    m_task = std::async(
        [this, output, root, path, url, size]() {
            quint64 current_size{0}, execed_last{0};
            std::string failed_file;
            alx::datetime heart_beat, total_cost;
            m_decoder->set_print_rtmsg(
                [this, size, &current_size, &execed_last, &failed_file, &heart_beat](const std::string& _file, quint64 _total, quint64 _current) {
                    if (_total == -1) {
                        failed_file = _file;
                        sig_show_execlog(QString(u8"\"%1\"提取失败，错误：%2")
                                             .arg(QString::fromLocal8Bit(failed_file.c_str(), failed_file.size()))
                                             .arg(alx::fpacker::decoder::error_message(_current)));
                    } else {
                        if (heart_beat.elapsed_ms() > 1000) {
                            quint64 execed = current_size + _current;
                            double spdms = double(execed - execed_last) / heart_beat.elapsed_ms();
                            execed_last = execed;
                            heart_beat.start();
                            emit sig_show_speed(spdms);
                            emit sig_show_execpb(execed * 100 / size);
                            emit sig_show_nedtime(double(size - execed) / spdms);
                            emit sig_show_message(QString("\"%1\" %2%")
                                                      .arg(QString::fromLocal8Bit(_file.c_str(), _file.size()))
                                                      .arg(_current * 100.0 / _total, 0, 'f', 2));
                        }
                        if (_total == _current) current_size += _current;
                    }
                });
            quint8 ret = m_decoder->save(root, path, alx::file_info(output.toLocal8Bit().toStdString()));
            if (ret == m_decoder->success) {
                emit sig_show_execpb(100);
                emit sig_show_message(QString(u8"数据\"%1\"导出到\"%2\"完成，耗时：%3")
                                          .arg(url)
                                          .arg(output)
                                          .arg(time2string(total_cost.elapsed_ms())));
            } else emit sig_show_message(QString(u8"数据\"%1\"导出失败，错误位置：\"%2\"，错误：%3")
                                             .arg(url)
                                             .arg(QString::fromLocal8Bit(failed_file.c_str(), failed_file.size()))
                                             .arg(alx::fpacker::decoder::error_message(ret)));

            emit sig_lock_window(false);
            ui.btn_exec->setText(u8"打开文件");
            m_runing = false;
        });
}

void DecodeWidget::expand_item() {
    if (nullptr == m_decoder) return;
    if (ui_selected.isValid()) ui.tv_fsys->expand(ui_selected);
    else ui.tv_fsys->expandAll();
}

void DecodeWidget::collapse_item() {
    if (nullptr == m_decoder) return;
    if (ui_selected.isValid()) ui.tv_fsys->collapse(ui_selected);
    else ui.tv_fsys->collapseAll();
}

void DecodeWidget::on_btn_exec_clicked() {
    if (ui.btn_exec->text() == u8"中断执行") {
        ui.btn_exec->setText(u8"打开文件");
        m_abort = true;
        if (m_task.valid()) m_task.get();
    } else {
        emit sig_lock_window(true);
        if (m_fpath.isEmpty()) return emit sig_show_message(u8"输入文件路径为空");
        alx::fpacker::decoder* temp_decoder = new alx::fpacker::decoder(
            new alx::istream_file(alx::file_info(m_fpath.toLocal8Bit().toStdString())),
            ui.le_password->text().toStdString());
        if (temp_decoder->valid()) {
            ui_fsys->setFSys(temp_decoder->fsystem());
            ui.tv_fsys->sortByColumn(0, Qt::SortOrder::AscendingOrder);
            delete m_decoder;
            m_decoder = temp_decoder;
            m_decoder->set_abort_flag(&m_abort);
            emit sig_show_message(QString(u8"文件\"%1\"打开成功").arg(m_fpath));
            emit sig_show_execpb(0);
            update_message();
        } else {
            if (temp_decoder->is_encrypt())
                if (ui.le_password->text().isEmpty())
                    emit sig_show_message(QString(u8"【加密文件】请输入密码再尝试打开"));
                else emit sig_show_message(QString(u8"【加密文件】输入密钥不正确"));
            else emit sig_show_message(QString(u8"无法打开文件"));
            delete temp_decoder;
        }
        emit sig_lock_window(false);
    }
}

void DecodeWidget::on_tv_fsys_customContextMenuRequested(const QPoint& _pos) {
    if (nullptr == m_decoder || m_runing) return;
    QModelIndex _index = ui.tv_fsys->indexAt(_pos);
    if (_index.isValid()) {
        ui_selected = _index;
        ui_act_export->setVisible(true);
        ui_act_export->setText(QString(u8"导出\"%1\"").arg(_index.data(0).toString()));
        if (_index.model()->hasChildren(_index)) {
            if (ui.tv_fsys->isExpanded(_index)) {
                ui_act_expand->setVisible(false);
                ui_act_collapse->setText(QString(u8"折叠\"%1\"").arg(_index.data(0).toString()));
                ui_act_collapse->setVisible(true);
            } else {
                ui_act_collapse->setVisible(false);
                ui_act_expand->setText(QString(u8"展开\"%1\"").arg(_index.data(0).toString()));
                ui_act_expand->setVisible(true);
            }
        } else {
            ui_act_expand->setVisible(false);
            ui_act_collapse->setVisible(false);
        }
    } else {
        ui_selected = QModelIndex();
        ui_act_export->setVisible(false);
        ui_act_expand->setVisible(false);
        ui_act_collapse->setVisible(false);
        ui_act_export_all->setVisible(true);
        ui_act_expand_all->setVisible(true);
        ui_act_collapse_all->setVisible(true);
    }
    ui_menu->exec(ui.tv_fsys->viewport()->mapToGlobal(_pos));
}
