var UI = (function () {
    'use strict';

    function el(tag, className, text) {
        const node = document.createElement(tag);
        if (className) node.className = className;
        if (text !== undefined) node.textContent = text;
        return node;
    }

    function formatSize(bytes) {
        if (bytes === 0) return '0 B';
        if (bytes === undefined || bytes === null || bytes < 0) return '—';
        const units = ['B', 'KB', 'MB', 'GB', 'TB'];
        const index = Math.min(units.length - 1, Math.floor(Math.log(bytes) / Math.log(1024)));
        const value = bytes / Math.pow(1024, index);
        return (index === 0 ? String(Math.round(value)) : value.toFixed(1)) + ' ' + units[index];
    }

    function formatSpeed(bytesPerSecond) {
        return formatSize(bytesPerSecond) + '/s';
    }

    function formatEta(seconds) {
        if (!isFinite(seconds) || seconds < 0) return '—';
        if (seconds < 60) return Math.max(1, Math.round(seconds)) + 's';
        if (seconds < 3600) return Math.round(seconds / 60) + 'm';
        return (seconds / 3600).toFixed(1) + 'h';
    }

    function formatTime(text) {
        return text ? String(text).slice(0, 16) : '—';
    }

    function toast(message, type) {
        const container = document.getElementById('toasts');
        const node = el('div', 'toast' + (type ? ' ' + type : ''), message);
        container.appendChild(node);
        setTimeout(function () {
            node.style.opacity = '0';
            node.style.transition = 'opacity .25s';
            setTimeout(function () { node.remove(); }, 250);
        }, 3200);
    }

    // ----- Modal -----
    let modalBox = null;
    let modalTitle = null;
    let modalText = null;
    let modalPath = null;
    let modalOk = null;
    let modalResolver = null;

    function initModal() {
        modalBox = document.getElementById('modal');
        modalTitle = document.getElementById('modalTitle');
        modalText = document.getElementById('modalText');
        modalPath = document.getElementById('modalPath');
        modalOk = document.getElementById('modalOk');
        const cancel = document.getElementById('modalCancel');
        cancel.addEventListener('click', function () { finishModal(false); });
        modalOk.addEventListener('click', function () { finishModal(true); });
        modalBox.addEventListener('click', function (event) {
            if (event.target === modalBox) finishModal(false);
        });
    }

    function finishModal(value) {
        if (!modalBox || modalBox.hidden) return;
        modalBox.hidden = true;
        if (modalResolver) {
            const resolve = modalResolver;
            modalResolver = null;
            resolve(value);
        }
    }

    function modal(options) {
        options = options || {};
        return new Promise(function (resolve) {
            if (modalResolver) {
                const previous = modalResolver;
                modalResolver = null;
                previous(false);
            }
            modalResolver = resolve;
            modalTitle.textContent = options.title || '';
            modalText.textContent = options.text || '';
            modalPath.textContent = options.path || '';
            modalPath.hidden = !options.path;
            modalOk.textContent = options.confirm || I18N.t('modal.confirm');
            modalOk.className = 'btn ' + (options.danger === false ? 'btn-primary' : 'btn-danger');
            modalBox.hidden = false;
            modalOk.focus();
        });
    }

    function isModalOpen() {
        return modalBox && !modalBox.hidden;
    }

    // ----- Dock -----
    function addDockItem(name, direction) {
        const dock = document.getElementById('dock');
        const item = el('div', 'dock-item');
        const icon = el('div', 'dock-icon', direction === 'up' ? '⇧' : '⇩');
        const body = el('div', 'dock-body');
        const head = el('div', 'dock-head');
        const title = el('div', 'dock-name', name);
        title.title = name;
        const cancel = el('button', 'icon-btn', '✕');
        cancel.title = I18N.t('dock.cancel');
        cancel.setAttribute('aria-label', cancel.title);
        head.appendChild(title);
        head.appendChild(cancel);

        const bar = el('div', 'dock-bar');
        const fill = el('div', 'dock-fill');
        bar.appendChild(fill);
        const status = el('div', 'dock-status', I18N.t('dock.waiting'));

        body.appendChild(head);
        body.appendChild(bar);
        body.appendChild(status);
        item.appendChild(icon);
        item.appendChild(body);
        dock.appendChild(item);
        dock.hidden = false;

        let removed = false;
        function remove() {
            if (removed) return;
            removed = true;
            item.remove();
            if (!dock.children.length) dock.hidden = true;
        }

        const handle = {
            update: function (percent, text) {
                fill.style.width = Math.max(0, Math.min(100, percent)) + '%';
                if (text !== undefined) status.textContent = text;
            },
            done: function (text) {
                handle.update(100, text || '');
                status.classList.add('done');
                setTimeout(remove, 800);
            },
            fail: function (text) {
                if (text) status.textContent = text;
                status.classList.add('error');
                setTimeout(remove, 2400);
            },
            onCancel: function (fn) {
                cancel.addEventListener('click', function () { fn(); });
            },
            remove: remove
        };
        return handle;
    }

    // ----- Viewer -----
    let viewerOpen = false;
    let viewerClose = null;

    function openViewer(options) {
        const viewer = document.getElementById('viewer');
        document.getElementById('viewerTitle').textContent = options.title || '';
        document.getElementById('viewerIcon').textContent = options.icon || '';
        document.getElementById('viewerMeta').textContent = options.meta || '';
        const body = document.getElementById('viewerBody');
        body.textContent = '';
        const download = document.getElementById('viewerDownload');
        download.hidden = !options.onDownload;
        download.onclick = options.onDownload || null;
        viewerClose = options.onClose || null;
        viewerOpen = true;
        viewer.hidden = false;
        document.getElementById('viewerClose').focus();
        return body;
    }

    function closeViewer() {
        if (!viewerOpen) return;
        viewerOpen = false;
        document.getElementById('viewer').hidden = true;
        document.getElementById('viewerBody').textContent = '';
        const callback = viewerClose;
        viewerClose = null;
        if (callback) callback();
    }

    function isViewerOpen() {
        return viewerOpen;
    }

    // ----- Theme -----
    function currentTheme() {
        return document.documentElement.getAttribute('data-theme') === 'dark' ? 'dark' : 'light';
    }

    function applyTheme(theme) {
        document.documentElement.setAttribute('data-theme', theme);
        try { localStorage.setItem('hfs.theme', theme); } catch (e) { }
        const button = document.getElementById('themeToggle');
        if (button) {
            button.textContent = theme === 'dark' ? '☀' : '◐';
            button.title = I18N.t('theme.toggle');
            button.setAttribute('aria-label', button.title);
        }
    }

    function toggleTheme() {
        applyTheme(currentTheme() === 'dark' ? 'light' : 'dark');
    }

    function init() {
        initModal();
        document.getElementById('viewerClose').addEventListener('click', closeViewer);
        document.getElementById('viewer').addEventListener('click', function (event) {
            if (event.target === document.getElementById('viewer')) closeViewer();
        });
        document.addEventListener('keydown', function (event) {
            if (event.key !== 'Escape') return;
            if (isModalOpen()) {
                event.preventDefault();
                event.stopImmediatePropagation();
                finishModal(false);
            } else if (viewerOpen) {
                event.preventDefault();
                event.stopImmediatePropagation();
                closeViewer();
            }
        });
    }

    return {
        el: el,
        init: init,
        formatSize: formatSize,
        formatSpeed: formatSpeed,
        formatEta: formatEta,
        formatTime: formatTime,
        toast: toast,
        modal: modal,
        isModalOpen: isModalOpen,
        addDockItem: addDockItem,
        openViewer: openViewer,
        closeViewer: closeViewer,
        isViewerOpen: isViewerOpen,
        currentTheme: currentTheme,
        applyTheme: applyTheme,
        toggleTheme: toggleTheme
    };
})();
