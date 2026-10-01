// Copyright (c) 2026 AlexisVon

#include "EncodeWidget.h"

#include <QDragEnterEvent>
#include <QMimeData>

#include <afpacker.h>
#include <astream_ex.h>

EncodeWidget::EncodeWidget(QWidget* parent)
    : LZAWidget(parent) {
    ui.setupUi(this);
    init_ui();
    init_signal();
}

EncodeWidget::~EncodeWidget() {
    m_abort = true;
    if (m_task.valid()) m_task.get();
}

void EncodeWidget::init_ui() {
    setAcceptDrops(true);
    ui.fm_password->setEnabled(false);
    ui.te_exec_log->setVisible(false);
    ui.tw_path_list->setColumnCount(5);
    ui.tw_path_list->setSelectionMode(QAbstractItemView::NoSelection);
    ui.tw_path_list->setHorizontalHeaderLabels({u8"路径", u8"名称", u8"类型", u8"大小", u8"移除"});
    ui.tw_path_list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui.tw_path_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui.tw_path_list->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui.tw_path_list->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui.tw_path_list->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
}

void EncodeWidget::init_signal() {
    connect(this, &EncodeWidget::sig_lock_window, this, &EncodeWidget::lock_window);
    connect(this, &EncodeWidget::sig_show_execpb, this, [this](int _val) { ui.pb_exec->setValue(_val); });
    connect(this, &EncodeWidget::sig_show_execlog, this, [this](const QString& _log) {ui.te_exec_log->setVisible(true); ui.te_exec_log->append(_log); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(ui.cb_password, &QCheckBox::checkStateChanged, this,
            [this](Qt::CheckState _s) { ui.le_password->setEchoMode(_s == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password); });
#else
    connect(ui.cb_password, &QCheckBox::stateChanged, this,
            [this](int _s) { ui.le_password->setEchoMode(_s == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password); });
#endif
    connect(ui.cb_encrypt, &QCheckBox::toggled, this, &EncodeWidget::on_cb_encrypt_toggled);
    connect(ui.tw_path_list, &QTableWidget::cellChanged, this,
            [this](int _row, int _col) {
                if (_col != 1) return;
                QTableWidgetItem* item = ui.tw_path_list->item(_row, _col);
                if (!item) return;
                QString name = item->text();
                if (name.isEmpty() || name == "." || name.contains("..") || name.contains('/') || name.contains('\\')) {
                    emit sig_show_message(u8"仅允许输入文件名，不支持路径分隔符");
                    item->setText(item->toolTip());
                }
            });
}

void EncodeWidget::update_message() {
    quint64 total_size{0};
    for (int i = 0; i < ui.tw_path_list->rowCount(); i++) total_size += ui.tw_path_list->item(i, 0)->data(1).toULongLong();
    emit sig_show_srcsize(total_size);
    emit sig_show_lzasize(0);
    emit sig_show_speed(0.0);
}

void EncodeWidget::lock_window(bool _lock) {
    _lock = !_lock;
    ui.fm_option->setEnabled(_lock);
    ui.btn_addfile->setEnabled(_lock);
    ui.btn_addfolder->setEnabled(_lock);
    ui.btn_clear->setEnabled(_lock);
    ui.tw_path_list->setEditTriggers(_lock ? QAbstractItemView::NoEditTriggers : QAbstractItemView::DoubleClicked);
}

void EncodeWidget::add_path_item(const QString& _path) {
    QFileInfo info(_path);
    quint64 size{0};
    if (info.isSymLink()) return emit sig_show_message(u8"不支持添加文件链接");
    if (info.isFile()) size = info.size();
    else size = alx::file_info::calsize(alx::file_info(_path.toLocal8Bit().toStdString()), true);

    int index = ui.tw_path_list->rowCount();
    ui.tw_path_list->setRowCount(index + 1);
    auto new_table_item = [](const QString& _text, bool _edit) -> QTableWidgetItem* {
        QTableWidgetItem* item = new QTableWidgetItem(_text);
        if (!_edit) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    };

    QTableWidgetItem* item = new_table_item(info.absoluteFilePath(), false);
    ui.tw_path_list->setItem(index, 0, item);
    item->setData(1, size);
    QTableWidgetItem* name_item = new_table_item(info.fileName(), true);
    name_item->setToolTip(info.fileName());
    ui.tw_path_list->setItem(index, 1, name_item);
    ui.tw_path_list->setItem(index, 2, new_table_item(info.isFile() ? u8"文件" : u8"目录", false));
    ui.tw_path_list->setItem(index, 3, new_table_item(size2string(size), false));
    QPushButton* btn_remove = new QPushButton(u8"移除");
    connect(btn_remove, &QPushButton::clicked, this, [item, this]() {
        if (m_runing) return;
        for (int i = 0; i < ui.tw_path_list->rowCount(); i++)
            if (ui.tw_path_list->item(i, 0) == item) {
                ui.tw_path_list->removeRow(i);
                return update_message();
            }
    });
    ui.tw_path_list->setCellWidget(index, 4, btn_remove);
}

void EncodeWidget::exec_impl(QList<QPair<QString, QString>> _path_list, quint64 _total_size) {
    alx::fpacker::encoder packer(ui.cb_compress->isChecked(),
                                 ui.cb_encrypt->isChecked() ? ui.le_password->text().toStdString() : std::string());
    packer.set_abort_flag(&m_abort);
    packer.set_igerr_linkfile(true);
    packer.set_igerr_notexist(ui.cb_igerr->isChecked());
    quint64 current_size{0}, execed_last{0};
    std::string failed_file;
    alx::datetime heart_beat, total_cost;
    packer.set_print_rtmsg(
        [this, _total_size, &current_size, &execed_last, &failed_file, &heart_beat, &packer](const std::string& _file, quint64 _total, quint64 _current) {
            if (_total == -1) {
                failed_file = _file;
                sig_show_execlog(QString(u8"\"%1\"编码失败，错误：%2")
                                     .arg(QString::fromLocal8Bit(failed_file.c_str(), failed_file.size()))
                                     .arg(alx::fpacker::encoder::error_message(_current)));
            } else {
                if (heart_beat.elapsed_ms() > 1000) {
                    quint64 execed = current_size + _current;
                    double spdms = double(execed - execed_last) / heart_beat.elapsed_ms();
                    execed_last = execed;
                    heart_beat.start();
                    emit sig_show_speed(spdms);
                    emit sig_show_lzasize(packer.get_stream()->total());
                    emit sig_show_execpb(execed * 100.0 / _total_size);
                    emit sig_show_nedtime(double(_total_size - execed) / spdms);
                    emit sig_show_message(QString("\"%1\" %2%")
                                              .arg(QString::fromLocal8Bit(_file.c_str(), _file.size()))
                                              .arg(_current * 100.0 / _total, 0, 'f', 2));
                }
                if (_total == _current) current_size += _current;
            }
        });
    if (packer.start(
            new alx::ostream_file(
                alx::file_info(m_fpath.toLocal8Bit().toStdString()),
                false))) {
        quint8 ret{packer.success};
        for (const auto& path : _path_list) {
            ret = packer.append(alx::file_info(path.first.toLocal8Bit().toStdString()), path.second.toLocal8Bit().toStdString());
            if (ret != packer.success) break;
        }
        if (ret == packer.success) {
            if (packer.finish()) {
                emit sig_show_execpb(100);
                emit sig_show_lzasize(QFileInfo(m_fpath).size());
                emit sig_show_message(QString(u8"文件\"%1\"写入完成，耗时：%2")
                                          .arg(m_fpath)
                                          .arg(time2string(total_cost.elapsed_ms())));
            } else emit sig_show_message(QString(u8"文件\"%1\"写入失败，文件尾编码失败").arg(m_fpath));
        } else emit sig_show_message(QString(u8"文件\"%1\"写入失败，错误位置：\"%2\"，错误：%3")
                                         .arg(m_fpath)
                                         .arg(QString::fromLocal8Bit(failed_file.c_str(), failed_file.size()))
                                         .arg(alx::fpacker::encoder::error_message(ret)));
    } else emit sig_show_message(QString(u8"文件\"%1\"无法写入").arg(m_fpath));

    emit sig_lock_window(false);
    ui.btn_exec->setText(u8"开始执行");
    m_runing = false;
}

void EncodeWidget::on_btn_exec_clicked() {
    if (ui.btn_exec->text() == u8"中断执行") {
        ui.btn_exec->setText(u8"开始执行");
        m_abort = true;
        if (m_task.valid()) m_task.get();
    } else {
        if (m_fpath.isEmpty()) return emit sig_show_message(QString(u8"输出文件路径为空"));
        QList<QPair<QString, QString>> path_list;
        QSet<QString> filter;
        quint64 total_size{0};
        for (int i = 0; i < ui.tw_path_list->rowCount(); i++) {
            QPair<QString, QString> temp = {ui.tw_path_list->item(i, 0)->text(), ui.tw_path_list->item(i, 1)->text()};
            if (filter.contains(temp.second)) return emit sig_show_message(QString(u8"第%1行出现重复名称").arg(i + 1));
            path_list.append(temp);
            filter.insert(temp.second);
            total_size += ui.tw_path_list->item(i, 0)->data(1).toULongLong();
        }

        emit sig_lock_window(true);
        emit sig_show_execpb(0);
        ui.te_exec_log->clear();
        ui.te_exec_log->setVisible(false);
        ui.btn_exec->setText(u8"中断执行");
        m_abort = false;
        m_runing = true;
        m_task = std::async(std::bind(&EncodeWidget::exec_impl, this, path_list, total_size));
    }
}

void EncodeWidget::on_btn_addfile_clicked() {
    static QString last_path;
    QStringList paths = QFileDialog::getOpenFileNames(this, u8"选择要添加的文件(可多选)",
                                                      last_path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) : last_path);
    if (paths.isEmpty()) return;
    for (const auto& path : paths) add_path_item(path);
    last_path = paths.last();
    update_message();
}

void EncodeWidget::on_btn_addfolder_clicked() {
    static QString last_path;
    QString path = QFileDialog::getExistingDirectory(this, u8"选择要添加的文件夹",
                                                     last_path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) : last_path);
    if (path.isEmpty()) return;
    add_path_item(last_path = path);
    update_message();
}

void EncodeWidget::on_btn_clear_clicked() {
    ui.tw_path_list->setRowCount(0);
    ui.tw_path_list->clearContents();
    ui.te_exec_log->clear();
    ui.te_exec_log->setVisible(false);
    ui.pb_exec->setValue(0);
    update_message();
}

void EncodeWidget::on_cb_encrypt_toggled(bool _checked) {
    ui.fm_password->setEnabled(_checked);
}

void EncodeWidget::dragEnterEvent(QDragEnterEvent* _ev) {
    if (_ev->mimeData()->hasUrls()) {
        _ev->acceptProposedAction();
    }
}

void EncodeWidget::dropEvent(QDropEvent* _ev) {
    if (_ev->mimeData()->hasUrls()) {
        const QList<QUrl> urls = _ev->mimeData()->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                QString filePath = url.toLocalFile();
                add_path_item(filePath);
            }
        }
        update_message();
        _ev->acceptProposedAction();
    }
}
