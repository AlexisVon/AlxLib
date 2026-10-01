// Copyright (c) 2026 AlexisVon

#include "FSysTree.h"
#include "Global.h"

#include <QDataStream>

FSysTreeModel::FSysTreeModel(QObject* _parent)
    : QAbstractItemModel(_parent), m_root(new FSysTreeItem()) {
}

FSysTreeModel::~FSysTreeModel() {
    delete m_root;
}

QVariant FSysTreeModel::data(const QModelIndex& _index, int _role) const {
    return !_index.isValid() || _role != Qt::DisplayRole ? QVariant() : static_cast<FSysTreeItem*>(_index.internalPointer())->data(_index.column());
}

Qt::ItemFlags FSysTreeModel::flags(const QModelIndex& _index) const {
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(_index);
    return _index.isValid() ? Qt::ItemIsDragEnabled | defaultFlags : defaultFlags;
}

QVariant FSysTreeModel::headerData(int _section, Qt::Orientation _orientation, int _role) const {
    return Qt::Horizontal == _orientation && Qt::DisplayRole == _role ? 0 == _section ? u8"名称" : 1 == _section ? u8"类型"
                                                                                               : 2 == _section   ? u8"大小"
                                                                                                                 : QVariant()
                                                                      : QVariant();
}

QModelIndex FSysTreeModel::index(int _row, int _column, const QModelIndex& _parent) const {
    if (!hasIndex(_row, _column, _parent)) return QModelIndex();
    FSysTreeItem* item = !_parent.isValid() ? m_root->child(_row) : static_cast<FSysTreeItem*>(_parent.internalPointer())->child(_row);
    return nullptr != item ? createIndex(_row, _column, item) : QModelIndex();
}

QModelIndex FSysTreeModel::parent(const QModelIndex& _index) const {
    if (!_index.isValid()) return QModelIndex();

    FSysTreeItem* item = static_cast<FSysTreeItem*>(_index.internalPointer())->parentItem();
    return nullptr != item ? createIndex(item->row(), 0, item) : QModelIndex();
}

int FSysTreeModel::rowCount(const QModelIndex& _parent) const {
    return _parent.column() > 0 ? 0 : !_parent.isValid() ? m_root->childCount()
                                                         : static_cast<FSysTreeItem*>(_parent.internalPointer())->childCount();
}

int FSysTreeModel::columnCount(const QModelIndex& _parent) const {
    (void) _parent;
    return 3;
}

void FSysTreeModel::setFSys(const alx::varmap& _fsys) {
    beginResetModel();
    delete m_root;
    m_root = new FSysTreeItem();
    setupModelData(m_root, _fsys);
    endResetModel();
}

void FSysTreeModel::setFSys(const alx::variant& _fval) {
    beginResetModel();
    delete m_root;
    m_root = new FSysTreeItem();
    setupModelData(m_root, _fval);
    endResetModel();
}

void FSysTreeModel::sort(int _column, Qt::SortOrder _order) {
    if (!m_root || m_root->childCount() == 0) return;

    beginResetModel();
    sortChildren(m_root, _column, _order);
    endResetModel();
}

void FSysTreeModel::setupModelData(FSysTreeItem* _parent, const alx::variant& _value) {
    switch (_value.type()) {
    case alx::variant::id<alx::varmap>(): {
        const alx::varmap& vmap = _value.to<alx::varmap>();
        for (const auto& it : vmap) {
            FSysTreeItem* item = new FSysTreeItem(_parent);
            item->setData(*it.second, it.first);
            _parent->appendChild(item);

            if (it.second->is<alx::varmap>())
                setupModelData(item, *it.second);
        }
    } break;
    default: break;
    }
}

void FSysTreeModel::sortChildren(FSysTreeItem* parent, int column, Qt::SortOrder order) {
    if (!parent || parent->childCount() <= 1) return;

    std::sort(parent->m_childs.begin(), parent->m_childs.end(),
              [column, order](FSysTreeItem* left, FSysTreeItem* right) {
                  return compareItems(left, right, column, order);
              });

    for (auto& child : parent->m_childs) {
        sortChildren(child, column, order);
    }
}

bool FSysTreeModel::compareItems(FSysTreeItem* _left, FSysTreeItem* _right, int _column, Qt::SortOrder _order) {
    if (nullptr == _left || nullptr == _right) return false;

    bool result =
        _column == 0 || _column == 1 ? _left->data(_column).toString().localeAwareCompare(_right->data(_column).toString()) < 0 : _column == 2 ? _left->size() < _right->size()
                                                                                                                                               : true;
    return _order == Qt::AscendingOrder ? result : !result;
}

FSysTreeItem::FSysTreeItem(FSysTreeItem* _parent)
    : m_parent(_parent), m_size(-1) {
}

FSysTreeItem::~FSysTreeItem() {
    qDeleteAll(m_childs);
}

void FSysTreeItem::appendChild(FSysTreeItem* _child) {
    m_childs.append(_child);
}

FSysTreeItem* FSysTreeItem::child(int _row) const {
    return _row < 0 || _row >= m_childs.size() ? nullptr : m_childs.at(_row);
}

int FSysTreeItem::childCount() const {
    return m_childs.count();
}

int FSysTreeItem::columnCount() const {
    return 2;
}

QVariant FSysTreeItem::data(int _column) const {
    if (0 == _column) return m_key;
    else if (1 == _column) {
        switch (m_value.type()) {
        case alx::variant::id<alx::varmap>(): return u8"目录";
        case alx::variant::id<alx::varvec>(): return u8"文件";
        default: break;
        }
    } else if (2 == _column) return size2string(size());
    return QVariant();
}

quint64 FSysTreeItem::size() const {
    if (m_size != -1) return m_size;

    m_size = 0;
    if (m_childs.empty()) {
        if (m_value.type() == alx::variant::id<alx::varvec>()) {
            const alx::varvec& blocks = m_value.to<alx::varvec>();
            m_size = blocks.size() & 0X01U ? blocks.back().to<quint64>() : 0;
        }
    } else
        for (auto it : m_childs) m_size += it->size();

    return m_size;
}

int FSysTreeItem::row() const {
    return nullptr != m_parent ? m_parent->m_childs.indexOf(const_cast<FSysTreeItem*>(this)) : 0;
}

FSysTreeItem* FSysTreeItem::parentItem() {
    return m_parent;
}

void FSysTreeItem::setData(const alx::variant& _value, const std::string& _key) {
    const std::string utf8 = alx::strutil::code_conver(_key, alx::strutil::UTF8);
    m_value = _value;
    m_key = QString::fromUtf8(utf8.c_str(), utf8.size());
}

std::string FSysTreeItem::getRoot() const {
    if (m_parent == nullptr) return std::string();
    else {
        const FSysTreeItem* root = this;
        while (root->m_parent != nullptr && root->m_parent->m_parent != nullptr) root = root->m_parent;
        return root->m_parent != nullptr ? root->m_key.toLocal8Bit().toStdString() : std::string();
    }
}

std::list<std::string> FSysTreeItem::getPath() const {
    if (m_parent == nullptr) return {};
    else {
        const FSysTreeItem* root = this;
        while (root->m_parent != nullptr && root->m_parent->m_parent != nullptr) root = root->m_parent;

        const FSysTreeItem* item = this;
        std::list<std::string> result;
        while (item != nullptr && item != root) {
            result.push_front(item->m_key.toLocal8Bit().toStdString());
            item = item->m_parent;
        }
        return result;
    }
}

QString FSysTreeItem::CalUrl(const std::string& _root, const std::list<std::string>& _path) {
    QString result = QString("%1:\\%2")
                         .arg(QString::fromLocal8Bit(_root.c_str(), _root.size()))
                         .arg(_path.empty() ? QString() : QString::fromLocal8Bit(_path.front().c_str(), _path.front().size()));
    for (auto iter = (++_path.cbegin()); iter != _path.cend(); iter++)
        result += QString("\\%1").arg(QString::fromLocal8Bit(iter->c_str(), iter->size()));
    return result;
}
