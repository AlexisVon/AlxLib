var I18N = (function () {
    'use strict';

    var dict = {
        en: {
            'brand.sub': 'AlxLib HTTP example',
            'theme.toggle': 'Toggle theme',
            'lang.toggle': 'Switch language',
            'toolbar.up': '↑ Up',
            'toolbar.refresh': '↻ Refresh',
            'toolbar.upload': '⇧ Upload',
            'toolbar.preview': '▤ Preview',
            'col.name': 'Name',
            'col.size': 'Size',
            'col.modified': 'Modified',
            'col.action': 'Action',
            'root': 'Root',
            'state.loading': 'Loading…',
            'state.empty': 'This folder is empty',
            'state.retry': 'Retry',
            'action.download': 'Download',
            'action.delete': 'Delete',
            'action.preview': 'Preview',
            'preview.download': 'Download',
            'preview.open': 'Open',
            'preview.toggle': 'Toggle preview pane',
            'preview.hide': 'Hide preview pane',
            'preview.close': 'Close',
            'preview.type': 'Type',
            'preview.size': 'Size',
            'preview.modified': 'Modified',
            'preview.location': 'Location',
            'preview.dimensions': 'Dimensions',
            'preview.items_label': 'Items',
            'preview.folder_type': 'File folder',
            'preview.disk_type': 'Drive',
            'preview.file_type': 'File',
            'preview.ext_type': '{ext} file',
            'preview.too_large': 'Too large to preview',
            'preview.truncated': 'Showing the first {size}',
            'preview.unsupported': 'No preview for this file type',
            'preview.image_limit': 'Image is larger than {size}; download it to view',
            'preview.empty': 'File is empty',
            'drop.hint': 'Drop files to upload',
            'modal.cancel': 'Cancel',
            'modal.confirm': 'Confirm',
            'modal.delete_title': 'Delete',
            'modal.delete_text': 'This will permanently delete:',
            'modal.delete_folder': 'Only empty folders can be deleted.',
            'modal.overwrite_title': 'File exists',
            'modal.overwrite_text': '"{name}" already exists. Overwrite it?',
            'modal.overwrite_ok': 'Overwrite',
            'toast.uploaded': 'Uploaded {name}',
            'toast.deleted': 'Deleted {name}',
            'toast.saved': 'Saved {name}',
            'toast.too_large': '{name} exceeds {size}',
            'status.items_one': '{count} item',
            'status.items_other': '{count} items',
            'status.selected_one': '{count} item selected',
            'status.selected_other': '{count} items selected',
            'dock.cancel': 'Cancel',
            'dock.eta': 'ETA {time}',
            'dock.waiting': 'Waiting…',
            'err.not_found': 'Not found',
            'err.not_dir': 'Not a directory',
            'err.not_file': 'Not a file',
            'err.bad_path': 'Invalid path',
            'err.bad_request': 'Invalid request',
            'err.forbidden': 'Forbidden',
            'err.block_out_of_range': 'Block out of range',
            'err.open_failed': 'Cannot open file',
            'err.exists': 'File already exists',
            'err.is_link': 'Symbolic links are not supported',
            'err.is_dir': 'A folder with that name exists',
            'err.not_empty': 'Folder is not empty',
            'err.bad_name': 'Invalid file name',
            'err.too_large': 'File exceeds the 64 MB limit',
            'err.write_failed': 'Write failed',
            'err.remove_failed': 'Delete failed',
            'err.aborted': 'Cancelled',
            'err.network': 'Network error',
            'err.unknown': 'Unknown error'
        },
        zh: {
            'brand.sub': 'AlxLib HTTP 示例',
            'theme.toggle': '切换主题',
            'lang.toggle': '切换语言',
            'toolbar.up': '↑ 上级',
            'toolbar.refresh': '↻ 刷新',
            'toolbar.upload': '⇧ 上传',
            'toolbar.preview': '▤ 预览',
            'col.name': '名称',
            'col.size': '大小',
            'col.modified': '修改时间',
            'col.action': '操作',
            'root': '根目录',
            'state.loading': '加载中…',
            'state.empty': '该目录为空',
            'state.retry': '重试',
            'action.download': '下载',
            'action.delete': '删除',
            'action.preview': '预览',
            'preview.download': '下载',
            'preview.open': '打开',
            'preview.toggle': '切换预览窗格',
            'preview.hide': '隐藏预览窗格',
            'preview.close': '关闭',
            'preview.type': '类型',
            'preview.size': '大小',
            'preview.modified': '修改时间',
            'preview.location': '位置',
            'preview.dimensions': '尺寸',
            'preview.items_label': '项目数',
            'preview.folder_type': '文件夹',
            'preview.disk_type': '驱动器',
            'preview.file_type': '文件',
            'preview.ext_type': '{ext} 文件',
            'preview.too_large': '文件过大，无法预览',
            'preview.truncated': '仅显示前 {size}',
            'preview.unsupported': '该类型不支持预览',
            'preview.image_limit': '图片大于 {size}，请下载后查看',
            'preview.empty': '文件为空',
            'drop.hint': '拖放文件以上传',
            'modal.cancel': '取消',
            'modal.confirm': '确认',
            'modal.delete_title': '删除',
            'modal.delete_text': '将永久删除：',
            'modal.delete_folder': '只能删除空文件夹。',
            'modal.overwrite_title': '文件已存在',
            'modal.overwrite_text': '"{name}" 已存在，要覆盖吗？',
            'modal.overwrite_ok': '覆盖',
            'toast.uploaded': '已上传 {name}',
            'toast.deleted': '已删除 {name}',
            'toast.saved': '已保存 {name}',
            'toast.too_large': '{name} 超过 {size}',
            'status.items_one': '{count} 项',
            'status.items_other': '{count} 项',
            'status.selected_one': '已选中 {count} 项',
            'status.selected_other': '已选中 {count} 项',
            'dock.cancel': '取消',
            'dock.eta': '剩余 {time}',
            'dock.waiting': '等待中…',
            'err.not_found': '未找到',
            'err.not_dir': '不是目录',
            'err.not_file': '不是文件',
            'err.bad_path': '无效路径',
            'err.bad_request': '无效请求',
            'err.forbidden': '禁止访问',
            'err.block_out_of_range': '分块超出范围',
            'err.open_failed': '无法打开文件',
            'err.exists': '文件已存在',
            'err.is_link': '不支持符号链接',
            'err.is_dir': '已存在同名文件夹',
            'err.not_empty': '文件夹非空',
            'err.bad_name': '无效文件名',
            'err.too_large': '文件超过 64 MB 限制',
            'err.write_failed': '写入失败',
            'err.remove_failed': '删除失败',
            'err.aborted': '已取消',
            'err.network': '网络错误',
            'err.unknown': '未知错误'
        }
    };

    var lang = 'en';
    try {
        var saved = localStorage.getItem('hfs.lang');
        if (saved === 'zh' || saved === 'en') lang = saved;
    } catch (e) { }

    function t(key, vars) {
        var table = dict[lang] || dict.en;
        var text = table[key];
        if (text === undefined) text = dict.en[key];
        if (text === undefined) text = key;
        if (vars) {
            Object.keys(vars).forEach(function (name) {
                text = text.split('{' + name + '}').join(String(vars[name]));
            });
        }
        return text;
    }

    function exists(key) {
        return (dict[lang] && dict[lang][key] !== undefined) || dict.en[key] !== undefined;
    }

    function apply(root) {
        root = root || document;
        root.querySelectorAll('[data-i18n]').forEach(function (node) {
            node.textContent = t(node.getAttribute('data-i18n'));
        });
        root.querySelectorAll('[data-i18n-title]').forEach(function (node) {
            node.title = t(node.getAttribute('data-i18n-title'));
            node.setAttribute('aria-label', node.title);
        });
    }

    function updateToggle() {
        var button = document.getElementById('langToggle');
        if (button) button.textContent = lang === 'zh' ? 'EN' : '中';
        document.documentElement.lang = lang === 'zh' ? 'zh-CN' : 'en';
    }

    function setLang(next) {
        lang = next === 'zh' ? 'zh' : 'en';
        try { localStorage.setItem('hfs.lang', lang); } catch (e) { }
        apply();
        updateToggle();
    }

    function toggle() {
        setLang(lang === 'zh' ? 'en' : 'zh');
    }

    return {
        t: t,
        exists: exists,
        apply: apply,
        setLang: setLang,
        toggle: toggle,
        updateToggle: updateToggle,
        get lang() { return lang; }
    };
})();
