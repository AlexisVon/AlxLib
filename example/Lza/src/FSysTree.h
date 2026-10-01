// Copyright (c) 2026 AlexisVon

#pragma once

#include <QAbstractItemModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>

#include <ajson.h>

class FSysTreeItem;

class FSysTreeModel
    : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit FSysTreeModel(QObject* _parent = nullptr);
    ~FSysTreeModel();

public:
    QVariant data(const QModelIndex& _index, int _role) const override;
    Qt::ItemFlags flags(const QModelIndex& _index) const override;
    QVariant headerData(int _section, Qt::Orientation _orientation,
                        int _role = Qt::DisplayRole) const override;
    QModelIndex index(int _row, int _column,
                      const QModelIndex& _parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& _index) const override;
    int rowCount(const QModelIndex& _parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& _parent = QModelIndex()) const override;

public:
    void setFSys(const alx::varmap& _fsys);
    void setFSys(const alx::variant& _fval);
    inline const FSysTreeItem* root() { return m_root; }

public:
    void sort(int _column, Qt::SortOrder _order = Qt::AscendingOrder);

private:
    static void setupModelData(FSysTreeItem* _parent, const alx::variant& _value);
    static void sortChildren(FSysTreeItem* parent, int column, Qt::SortOrder order);
    static bool compareItems(FSysTreeItem* left, FSysTreeItem* right, int column, Qt::SortOrder order);

private:
    FSysTreeItem* m_root{nullptr};
};

class FSysTreeItem {
public:
    friend class FSysTreeModel;
    explicit FSysTreeItem(FSysTreeItem* _parent = nullptr);
    ~FSysTreeItem();

    void appendChild(FSysTreeItem* _child);
    FSysTreeItem* child(int _row) const;
    int childCount() const;
    int columnCount() const;
    QVariant data(int _column) const;
    quint64 size() const;
    int row() const;
    FSysTreeItem* parentItem();
    void setData(const alx::variant& _value, const std::string& _key = std::string());
    std::string getRoot() const;
    std::list<std::string> getPath() const;
    static QString CalUrl(const std::string& _root, const std::list<std::string>& _path);

private:
    FSysTreeItem* m_parent{nullptr};
    QList<FSysTreeItem*> m_childs;
    QString m_key;
    alx::variant m_value;
    mutable quint64 m_size;
};
