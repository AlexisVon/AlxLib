(function () {
    'use strict';

    const MAX_UPLOAD = 64 * 1024 * 1024;
    const PANE_TEXT_MAX = 64 * 1024;
    const VIEWER_TEXT_MAX = 1024 * 1024;
    const IMAGE_PREVIEW_MAX = 16 * 1024 * 1024;

    const TEXT_EXT = ['txt', 'md', 'markdown', 'log', 'json', 'xml', 'csv', 'ini', 'conf', 'cfg',
        'yaml', 'yml', 'toml', 'c', 'cc', 'cpp', 'cxx', 'h', 'hh', 'hpp', 'js', 'mjs', 'ts',
        'css', 'html', 'htm', 'sh', 'bash', 'zsh', 'py', 'rb', 'go', 'rs', 'java', 'sql',
        'axc', 'gitignore', 'env'];
    const IMAGE_EXT = ['png', 'jpg', 'jpeg', 'gif', 'webp', 'bmp', 'ico', 'svg'];

    const state = {
        path: '/',
        root: { virtual: true, name: 'Root', path: '/' },
        canDelete: true,
        entries: [],
        sortBy: 'name',
        sortOrder: 'asc',
        status: 'loading',
        error: null,
        selected: null,
        paneVisible: true,
        previewKey: null,
        preview: null,
        previewLoading: false,
        viewerKey: null,
        viewerEntry: null
    };

    const els = {};
    let navToken = 0;
    let previewSeq = 0;
    let previewAbort = null;
    let uploadQueue = Promise.resolve();

    function byId(id) {
        return document.getElementById(id);
    }

    async function boot() {
        els.breadcrumb = byId('breadcrumb');
        els.fileList = byId('fileList');
        els.fileTable = byId('fileTable');
        els.listPane = byId('listPane');
        els.stateBox = byId('stateBox');
        els.hostLabel = byId('hostLabel');
        els.filePicker = byId('filePicker');
        els.uploadBtn = byId('uploadBtn');
        els.previewToggle = byId('previewToggle');
        els.previewPane = byId('previewPane');
        els.paneIcon = byId('paneIcon');
        els.paneTitle = byId('paneTitle');
        els.paneStage = byId('paneStage');
        els.paneInfo = byId('paneInfo');
        els.paneActions = byId('paneActions');
        els.statusItems = byId('statusItems');
        els.statusSelection = byId('statusSelection');
        els.dropOverlay = byId('dropOverlay');

        UI.init();
        I18N.apply();
        I18N.updateToggle();
        UI.applyTheme(UI.currentTheme());
        els.hostLabel.textContent = location.host;

        try {
            const info = await Api.root();
            state.root = {
                virtual: !!info.virtual,
                name: info.name || 'Root',
                path: info.path || '/'
            };
            state.canDelete = info.delete !== false;
        } catch (e) { }

        state.paneVisible = readPanePref();
        applyPane();
        bind();
        navigate('/');
    }

    function readPanePref() {
        try {
            const saved = localStorage.getItem('hfs.pane');
            if (saved === '0') return false;
            if (saved === '1') return true;
        } catch (e) { }
        return window.innerWidth > 900;
    }

    function applyPane() {
        els.previewPane.hidden = !state.paneVisible;
        els.previewToggle.classList.toggle('active', state.paneVisible);
        try { localStorage.setItem('hfs.pane', state.paneVisible ? '1' : '0'); } catch (e) { }
    }

    function togglePane() {
        state.paneVisible = !state.paneVisible;
        applyPane();
        if (state.paneVisible) renderPane();
    }

    function bind() {
        byId('themeToggle').addEventListener('click', function () { UI.toggleTheme(); });
        byId('langToggle').addEventListener('click', function () {
            I18N.toggle();
            refreshLanguage();
        });
        byId('upBtn').addEventListener('click', up);
        byId('refreshBtn').addEventListener('click', function () { navigate(state.path); });
        els.uploadBtn.addEventListener('click', function () { els.filePicker.click(); });
        els.previewToggle.addEventListener('click', togglePane);
        byId('paneClose').addEventListener('click', function () {
            state.paneVisible = false;
            applyPane();
        });
        els.filePicker.addEventListener('change', function () {
            const files = Array.prototype.slice.call(els.filePicker.files || []);
            els.filePicker.value = '';
            enqueueUploads(files);
        });
        els.breadcrumb.addEventListener('click', function (event) {
            const target = event.target.closest('[data-path]');
            if (target) navigate(target.getAttribute('data-path'));
        });
        els.fileTable.querySelectorAll('.sortable').forEach(function (header) {
            header.addEventListener('click', function () { setSort(header.getAttribute('data-sort')); });
        });
        els.fileList.addEventListener('click', onListClick);
        els.fileList.addEventListener('dblclick', onListDblClick);
        els.listPane.addEventListener('click', function (event) {
            if (event.target.closest('tr.file-row')) return;
            if (event.target.closest('thead')) return;
            if (event.target.closest('button')) return;
            clearSelection();
        });
        document.addEventListener('keydown', onKeyDown);
        bindDragDrop();
        window.addEventListener('beforeunload', function () {
            if (previewAbort) previewAbort.abort();
        });
    }

    function refreshLanguage() {
        I18N.apply();
        renderBreadcrumb();
        updateSortIndicators();
        updateStatusBar();
        updateUploadButton();
        if (state.status === 'ready') renderRows();
        else showState(state.status, state.error);
        renderPane();
        if (UI.isViewerOpen()) renderViewer();
    }

    function normalize(path) {
        let value = '/' + String(path || '/').replace(/\\/g, '/');
        value = value.replace(/\/+/g, '/');
        if (value.length > 1 && value.charAt(value.length - 1) === '/') value = value.slice(0, -1);
        return value || '/';
    }

    function join(dir, name) {
        return normalize((dir === '/' ? '' : dir) + '/' + name);
    }

    function parentOf(path) {
        if (path === '/') return '/';
        const index = path.lastIndexOf('/');
        return index <= 0 ? '/' : path.slice(0, index);
    }

    function lastSegment(path) {
        const parts = path.split('/').filter(Boolean);
        return parts.length ? parts[parts.length - 1] : '/';
    }

    function extensionOf(name) {
        const dot = name.lastIndexOf('.');
        return dot >= 0 ? name.slice(dot + 1).toLowerCase() : '';
    }

    function isSupported(name) {
        const ext = extensionOf(name);
        return TEXT_EXT.indexOf(ext) !== -1 || IMAGE_EXT.indexOf(ext) !== -1;
    }

    function iconOf(entry) {
        if (entry.type === 'folder') return '📁';
        if (entry.type === 'disk') return '💾';
        return IMAGE_EXT.indexOf(extensionOf(entry.name)) !== -1 ? '🖼' : '📄';
    }

    function typeLabel(entry) {
        if (entry.type === 'disk') return I18N.t('preview.disk_type');
        if (entry.type !== 'file') return I18N.t('preview.folder_type');
        const ext = extensionOf(entry.name);
        if (!ext) return I18N.t('preview.file_type');
        return I18N.t('preview.ext_type', { ext: ext.toUpperCase() });
    }

    function rootLabel() {
        return state.root.virtual ? I18N.t('root') : (state.root.name || 'Root');
    }

    function displayPath(webPath) {
        if (state.root.virtual || state.root.path === '/') return webPath;
        const base = state.root.path.replace(/[\\/]+$/, '');
        return webPath === '/' ? base : base + webPath;
    }

    function mimeOf(ext) {
        const map = {
            png: 'image/png',
            jpg: 'image/jpeg',
            jpeg: 'image/jpeg',
            gif: 'image/gif',
            webp: 'image/webp',
            bmp: 'image/bmp',
            ico: 'image/x-icon',
            svg: 'image/svg+xml'
        };
        return map[ext] || 'application/octet-stream';
    }

    async function navigate(path) {
        const token = ++navToken;
        state.path = normalize(path);
        state.status = 'loading';
        state.entries = [];
        state.error = null;
        state.selected = null;
        UI.closeViewer();
        resetPreview();
        updateRowSelection();
        renderBreadcrumb();
        updateUploadButton();
        updateSortIndicators();
        showState('loading');
        renderPane();
        updateStatusBar();

        try {
            const data = await Api.list(state.path);
            if (token !== navToken) return;
            state.entries = parseEntries(data);
            state.status = state.entries.length ? 'ready' : 'empty';
            if (state.status === 'ready') renderRows();
            else showState('empty');
            updateStatusBar();
            renderPane();
        } catch (error) {
            if (token !== navToken) return;
            state.status = 'error';
            state.error = error;
            showState('error', error);
            updateStatusBar();
            renderPane();
        }
    }

    function parseEntries(data) {
        return Object.keys(data).map(function (name) {
            const info = data[name] || {};
            return {
                name: name,
                type: info.type || 'file',
                size: typeof info.size === 'number' ? info.size : 0,
                time: info.last_write_time || ''
            };
        });
    }

    function up() {
        if (state.path !== '/') navigate(parentOf(state.path));
    }

    function setSort(key) {
        if (state.sortBy === key) state.sortOrder = state.sortOrder === 'asc' ? 'desc' : 'asc';
        else {
            state.sortBy = key;
            state.sortOrder = 'asc';
        }
        updateSortIndicators();
        if (state.status === 'ready') renderRows();
    }

    function updateSortIndicators() {
        ['name', 'size', 'time'].forEach(function (key) {
            const node = byId('sort-' + key);
            if (node) node.textContent = state.sortBy === key ? (state.sortOrder === 'asc' ? '↑' : '↓') : '';
        });
    }

    function canUploadHere() {
        return !(state.path === '/' && state.root.virtual);
    }

    function updateUploadButton() {
        els.uploadBtn.disabled = !canUploadHere()
            || state.status === 'loading'
            || state.status === 'error';
    }

    function sorted() {
        const entries = state.entries.slice();
        const locale = I18N.lang === 'zh' ? 'zh-CN' : 'en';
        entries.sort(function (a, b) {
            const aFile = a.type === 'file' ? 1 : 0;
            const bFile = b.type === 'file' ? 1 : 0;
            if (aFile !== bFile) return aFile - bFile;
            let diff = 0;
            if (state.sortBy === 'size') diff = a.size - b.size;
            else if (state.sortBy === 'time') diff = String(a.time).localeCompare(String(b.time));
            else diff = a.name.localeCompare(b.name, locale);
            return state.sortOrder === 'asc' ? diff : -diff;
        });
        return entries;
    }

    function renderBreadcrumb() {
        els.breadcrumb.textContent = '';
        const parts = state.path.split('/').filter(Boolean);

        const root = document.createElement('button');
        root.className = 'crumb' + (parts.length ? '' : ' current');
        root.dataset.path = '/';
        root.textContent = '🏠 ' + rootLabel();
        if (!state.root.virtual && state.root.path) root.title = state.root.path;
        els.breadcrumb.appendChild(root);

        const start = Math.max(0, parts.length - 5);
        if (start > 0) els.breadcrumb.appendChild(UI.el('span', 'crumb-sep', '/ …'));
        let accumulated = '';
        for (let i = start; i < parts.length; i++) {
            accumulated += '/' + parts[i];
            els.breadcrumb.appendChild(UI.el('span', 'crumb-sep', '/'));
            const crumb = document.createElement('button');
            crumb.className = 'crumb' + (i === parts.length - 1 ? ' current' : '');
            crumb.dataset.path = accumulated;
            crumb.textContent = parts[i];
            els.breadcrumb.appendChild(crumb);
        }
    }

    function renderRows() {
        els.fileList.textContent = '';
        if (!state.entries.length) {
            showState('empty');
            return;
        }
        els.stateBox.hidden = true;
        const fragment = document.createDocumentFragment();
        sorted().forEach(function (entry) { fragment.appendChild(rowOf(entry)); });
        els.fileList.appendChild(fragment);
        updateUploadButton();
        updateRowSelection();
    }

    function rowOf(entry) {
        const row = document.createElement('tr');
        row.className = 'file-row';
        row.dataset.name = entry.name;
        row.dataset.type = entry.type;

        const nameCell = document.createElement('td');
        nameCell.className = 'col-name';
        const icon = UI.el('span', 'file-icon', iconOf(entry));
        const name = document.createElement('button');
        name.className = 'file-name';
        name.textContent = entry.name;
        name.title = entry.name;
        nameCell.appendChild(icon);
        nameCell.appendChild(name);

        const sizeCell = document.createElement('td');
        sizeCell.className = 'col-size file-size';
        sizeCell.textContent = entry.type === 'file' ? UI.formatSize(entry.size) : '—';

        const timeCell = document.createElement('td');
        timeCell.className = 'col-time file-time';
        timeCell.textContent = UI.formatTime(entry.time);

        const actionCell = document.createElement('td');
        actionCell.className = 'col-actions';
        if (entry.type === 'file')
            actionCell.appendChild(actionButton('⇩', I18N.t('action.download'), 'download', false));
        if (entry.type !== 'disk' && state.canDelete)
            actionCell.appendChild(actionButton('🗑', I18N.t('action.delete'), 'delete', true));

        row.appendChild(nameCell);
        row.appendChild(sizeCell);
        row.appendChild(timeCell);
        row.appendChild(actionCell);
        return row;
    }

    function actionButton(text, label, action, danger) {
        const button = document.createElement('button');
        button.className = 'row-btn' + (danger ? ' danger' : '');
        button.textContent = text;
        button.title = label;
        button.setAttribute('aria-label', label);
        button.dataset.action = action;
        return button;
    }

    function onListClick(event) {
        const row = event.target.closest('tr.file-row');
        if (!row) return;
        const entry = findEntry(row.dataset.name);
        if (!entry) return;
        selectEntry(entry);

        const button = event.target.closest('button.row-btn');
        if (!button) return;
        if (button.dataset.action === 'download') download(entry);
        else if (button.dataset.action === 'delete') removeEntry(entry);
    }

    function onListDblClick(event) {
        if (event.target.closest('button.row-btn')) return;
        const row = event.target.closest('tr.file-row');
        if (!row) return;
        const entry = findEntry(row.dataset.name);
        if (entry) openEntry(entry);
    }

    function onKeyDown(event) {
        if (UI.isModalOpen() || UI.isViewerOpen()) return;
        const target = event.target;
        if (target && target.closest && target.closest('.app-header, .toolbar, .pane-actions')) return;
        const tag = (target && target.tagName || '').toLowerCase();
        if (tag === 'input' || tag === 'select' || tag === 'textarea') return;

        if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
            const list = sorted();
            if (!list.length) return;
            event.preventDefault();
            let index = -1;
            for (let i = 0; i < list.length; i++)
                if (list[i].name === state.selected) { index = i; break; }
            if (event.key === 'ArrowDown') index = index < 0 ? 0 : Math.min(list.length - 1, index + 1);
            else index = index < 0 ? list.length - 1 : Math.max(0, index - 1);
            selectEntry(list[index]);
            const row = els.fileList.querySelector('tr[data-name="' + CSS.escape(list[index].name) + '"]');
            if (row) row.scrollIntoView({ block: 'nearest' });
        } else if (event.key === 'Enter') {
            if (target && target.closest && target.closest('.row-btn')) return;
            const entry = selectedEntry();
            if (entry) {
                event.preventDefault();
                openEntry(entry);
            }
        } else if (event.key === 'Backspace') {
            event.preventDefault();
            up();
        } else if (event.key === 'Escape') {
            clearSelection();
        }
    }

    function findEntry(name) {
        for (let i = 0; i < state.entries.length; i++)
            if (state.entries[i].name === name) return state.entries[i];
        return null;
    }

    function selectedEntry() {
        return state.selected === null ? null : findEntry(state.selected);
    }

    function selectEntry(entry) {
        if (!entry) {
            clearSelection();
            return;
        }
        if (state.selected === entry.name) return;
        state.selected = entry.name;
        updateRowSelection();
        updateStatusBar();
        resetPreview();

        const key = join(state.path, entry.name);
        state.previewKey = key;
        if (entry.type !== 'file') {
            state.previewLoading = false;
            state.preview = { kind: 'folder' };
            renderPane();
            return;
        }
        state.previewLoading = true;
        renderPane();
        loadPreview(entry, key, previewSeq);
    }

    function clearSelection() {
        if (state.selected === null) {
            renderPane();
            return;
        }
        state.selected = null;
        updateRowSelection();
        updateStatusBar();
        resetPreview();
        renderPane();
    }

    function updateRowSelection() {
        els.fileList.querySelectorAll('tr.file-row').forEach(function (row) {
            row.classList.toggle('selected', row.dataset.name === state.selected);
        });
    }

    function resetPreview() {
        if (previewAbort) {
            previewAbort.abort();
            previewAbort = null;
        }
        ++previewSeq;
        if (state.preview && state.preview.url) URL.revokeObjectURL(state.preview.url);
        state.preview = null;
        state.previewKey = null;
        state.previewLoading = false;
    }

    function loadPreview(entry, key, seq) {
        const ext = extensionOf(entry.name);
        if (IMAGE_EXT.indexOf(ext) !== -1) {
            if (entry.size > IMAGE_PREVIEW_MAX) {
                state.previewLoading = false;
                state.preview = { kind: 'none', reason: 'image_limit' };
                renderPane();
                return;
            }
            const controller = new AbortController();
            previewAbort = controller;
            (async function () {
                try {
                    const chunks = [];
                    let index = 0;
                    let total = 1;
                    while (index < total) {
                        const part = await Api.readBlock(key, index, controller.signal);
                        index = part.index;
                        total = part.total;
                        chunks.push(part.data);
                        index++;
                    }
                    if (seq !== previewSeq) return;
                    state.previewLoading = false;
                    state.preview = {
                        kind: 'image',
                        url: URL.createObjectURL(new Blob(chunks, { type: mimeOf(ext) }))
                    };
                    renderPane();
                    renderViewer();
                } catch (error) {
                    if (seq !== previewSeq || (error && error.name === 'AbortError')) return;
                    state.previewLoading = false;
                    state.preview = { kind: 'error', error: error };
                    renderPane();
                    renderViewer();
                }
            })();
            return;
        }
        if (TEXT_EXT.indexOf(ext) !== -1) {
            const controller = new AbortController();
            previewAbort = controller;
            (async function () {
                try {
                    const part = await Api.readBlock(key, 0, controller.signal);
                    if (seq !== previewSeq) return;
                    const decoder = new TextDecoder('utf-8');
                    const text = decoder.decode(part.data, { stream: entry.size > part.data.byteLength });
                    state.previewLoading = false;
                    state.preview = { kind: 'text', text: text, fileTruncated: entry.size > part.data.byteLength };
                    renderPane();
                    renderViewer();
                } catch (error) {
                    if (seq !== previewSeq || (error && error.name === 'AbortError')) return;
                    state.previewLoading = false;
                    state.preview = { kind: 'error', error: error };
                    renderPane();
                    renderViewer();
                }
            })();
            return;
        }
        state.previewLoading = false;
        state.preview = { kind: 'none' };
        renderPane();
    }

    function renderPane() {
        if (!state.paneVisible) return;
        const entry = selectedEntry();
        if (!entry) {
            renderPaneSummary();
            return;
        }

        els.paneIcon.textContent = iconOf(entry);
        els.paneTitle.textContent = entry.name;
        els.paneTitle.title = entry.name;
        els.paneStage.textContent = '';

        if (entry.type !== 'file') {
            els.paneStage.appendChild(UI.el('div', 'pane-big-icon', entry.type === 'disk' ? '💾' : '📁'));
        } else if (state.previewLoading) {
            els.paneStage.appendChild(UI.el('div', 'spinner'));
        } else if (state.preview) {
            renderPreviewStage(els.paneStage, entry, state.preview, PANE_TEXT_MAX);
            hookImageDimensions(els.paneStage, entry);
        }

        renderInfo(entry);
        renderActions(entry);
    }

    function renderPaneSummary() {
        els.paneIcon.textContent = '📁';
        els.paneTitle.textContent = state.path === '/' ? rootLabel() : lastSegment(state.path);
        els.paneTitle.title = state.path === '/' && !state.root.virtual ? state.root.path : '';
        els.paneStage.textContent = '';
        els.paneStage.appendChild(UI.el('div', 'pane-big-icon', '📁'));
        els.paneInfo.textContent = '';
        addInfo('preview.items_label', String(state.entries.length));
        addInfo('preview.location', displayPath(state.path));
        els.paneActions.textContent = '';
    }

    function addInfo(labelKey, value) {
        const dt = document.createElement('dt');
        dt.textContent = I18N.t(labelKey);
        const dd = document.createElement('dd');
        dd.textContent = value;
        els.paneInfo.appendChild(dt);
        els.paneInfo.appendChild(dd);
    }

    function renderInfo(entry) {
        els.paneInfo.textContent = '';
        addInfo('preview.type', typeLabel(entry));
        if (entry.type === 'file') addInfo('preview.size', UI.formatSize(entry.size));
        addInfo('preview.modified', UI.formatTime(entry.time));
        addInfo('preview.location', displayPath(state.path));
        if (state.preview && state.preview.kind === 'image' && state.preview.width)
            addInfo('preview.dimensions', state.preview.width + ' × ' + state.preview.height);
    }

    function renderActions(entry) {
        els.paneActions.textContent = '';
        if (entry.type !== 'file') {
            els.paneActions.appendChild(paneButton(I18N.t('preview.open'), 'btn btn-primary', function () {
                openEntry(entry);
            }));
            return;
        }
        if (isSupported(entry.name))
            els.paneActions.appendChild(paneButton(I18N.t('preview.open'), 'btn btn-primary', function () {
                openEntry(entry);
            }));
        els.paneActions.appendChild(paneButton(I18N.t('preview.download'), 'btn btn-secondary', function () {
            download(entry);
        }));
    }

    function paneButton(label, className, onClick) {
        const button = UI.el('button', className, label);
        button.addEventListener('click', onClick);
        return button;
    }

    function renderPreviewStage(container, entry, preview, textLimit) {
        if (preview.kind === 'text') {
            const truncated = preview.fileTruncated || preview.text.length > textLimit;
            if (truncated)
                container.appendChild(UI.el('div', 'preview-note',
                    I18N.t('preview.truncated', { size: UI.formatSize(textLimit) })));
            if (!preview.text) {
                container.appendChild(UI.el('div', 'preview-empty', I18N.t('preview.empty')));
                return;
            }
            const pre = document.createElement('pre');
            pre.textContent = preview.text.slice(0, textLimit);
            container.appendChild(pre);
        } else if (preview.kind === 'image') {
            const img = document.createElement('img');
            img.src = preview.url;
            img.alt = entry.name;
            container.appendChild(img);
        } else if (preview.kind === 'none') {
            container.appendChild(UI.el('div', 'pane-type-badge', extensionOf(entry.name).toUpperCase() || '?'));
            container.appendChild(UI.el('div', 'preview-note',
                I18N.t(preview.reason === 'image_limit' ? 'preview.image_limit' : 'preview.unsupported',
                    { size: UI.formatSize(IMAGE_PREVIEW_MAX) })));
        } else if (preview.kind === 'error') {
            container.appendChild(UI.el('div', 'preview-note', errorText(preview.error)));
        }
    }

    function hookImageDimensions(container, entry) {
        if (!state.preview || state.preview.kind !== 'image') return;
        const img = container.querySelector('img');
        if (!img) return;
        const update = function () {
            if (!state.preview || state.preview.url !== img.src) return;
            if (!img.naturalWidth) return;
            state.preview.width = img.naturalWidth;
            state.preview.height = img.naturalHeight;
            renderInfo(entry);
        };
        if (img.complete) update();
        else img.addEventListener('load', update, { once: true });
    }

    function openEntry(entry) {
        if (!entry) return;
        if (entry.type !== 'file') {
            navigate(join(state.path, entry.name));
            return;
        }
        if (!isSupported(entry.name)) {
            download(entry);
            return;
        }
        UI.closeViewer();
        if (state.selected !== entry.name) selectEntry(entry);
        state.viewerKey = join(state.path, entry.name);
        state.viewerEntry = entry;
        UI.openViewer({
            title: entry.name,
            icon: iconOf(entry),
            meta: viewerMeta(entry),
            onDownload: function () { download(entry); },
            onClose: function () {
                state.viewerKey = null;
                state.viewerEntry = null;
            }
        });
        renderViewer();
    }

    function viewerMeta(entry) {
        return UI.formatSize(entry.size) + ' · ' + typeLabel(entry) + ' · ' + UI.formatTime(entry.time);
    }

    function renderViewer() {
        if (!UI.isViewerOpen() || !state.viewerKey) return;
        const body = byId('viewerBody');
        body.textContent = '';
        if (state.previewLoading || !state.preview || state.previewKey !== state.viewerKey) {
            body.appendChild(UI.el('div', 'spinner'));
            return;
        }
        renderPreviewStage(body, state.viewerEntry, state.preview, VIEWER_TEXT_MAX);
    }

    function showState(kind, error) {
        els.fileList.textContent = '';
        els.stateBox.textContent = '';
        els.stateBox.className = 'state';
        els.stateBox.hidden = false;
        updateUploadButton();

        if (kind === 'loading') {
            els.stateBox.appendChild(UI.el('div', 'spinner'));
            els.stateBox.appendChild(UI.el('p', '', I18N.t('state.loading')));
            return;
        }
        if (kind === 'empty') {
            els.stateBox.appendChild(UI.el('div', 'state-icon', '📂'));
            els.stateBox.appendChild(UI.el('p', '', I18N.t('state.empty')));
            return;
        }
        els.stateBox.appendChild(UI.el('div', 'state-icon', '⚠️'));
        els.stateBox.appendChild(UI.el('p', '', errorText(error)));
        const retry = UI.el('button', 'btn btn-secondary', I18N.t('state.retry'));
        retry.addEventListener('click', function () { navigate(state.path); });
        els.stateBox.appendChild(retry);
    }

    function errorText(error) {
        if (!error) return I18N.t('err.unknown');
        if (error.name === 'AbortError') return I18N.t('err.aborted');
        const key = 'err.' + (error.code || 'unknown');
        if (I18N.exists(key)) return I18N.t(key);
        if (error instanceof TypeError) return I18N.t('err.network');
        return I18N.t('err.unknown');
    }

    function updateStatusBar() {
        const count = state.entries.length;
        els.statusItems.textContent = I18N.t(count === 1 ? 'status.items_one' : 'status.items_other', { count: count });
        els.statusSelection.textContent = state.selected === null
            ? ''
            : I18N.t('status.selected_one', { count: 1 });
    }

    // ----- Download -----
    async function download(entry) {
        const path = join(state.path, entry.name);
        const controller = new AbortController();
        const item = UI.addDockItem(entry.name, 'down');
        item.onCancel(function () { controller.abort(); });

        const chunks = [];
        let received = 0;
        let lastBytes = 0;
        let lastTime = Date.now();
        let speed = 0;
        let eta = 0;

        try {
            let index = 0;
            let total = 1;
            while (index < total) {
                const part = await Api.readBlock(path, index, controller.signal);
                index = part.index;
                total = part.total;
                chunks.push(part.data);
                received += part.data.byteLength;

                const now = Date.now();
                if (now - lastTime >= 500) {
                    speed = (received - lastBytes) * 1000 / (now - lastTime);
                    lastBytes = received;
                    lastTime = now;
                    eta = speed > 0 ? Math.max(0, (entry.size - received) / speed) : 0;
                }
                const percent = entry.size > 0 ? Math.min(100, received * 100 / entry.size) : 100;
                let text = percent.toFixed(0) + '%';
                if (speed > 0) {
                    text += ' · ' + UI.formatSpeed(speed);
                    if (eta > 0) text += ' · ' + I18N.t('dock.eta', { time: UI.formatEta(eta) });
                }
                item.update(percent, text);
                index++;
            }

            saveBlob(new Blob(chunks, { type: 'application/octet-stream' }), entry.name);
            item.done(I18N.t('toast.saved', { name: entry.name }));
            UI.toast(I18N.t('toast.saved', { name: entry.name }), 'success');
        } catch (error) {
            if (error && (error.name === 'AbortError' || error.code === 'aborted'))
                item.fail(I18N.t('err.aborted'));
            else item.fail(errorText(error));
        }
    }

    function saveBlob(blob, name) {
        const url = URL.createObjectURL(blob);
        const link = document.createElement('a');
        link.href = url;
        link.download = name;
        document.body.appendChild(link);
        link.click();
        link.remove();
        setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
    }

    // ----- Delete -----
    async function removeEntry(entry) {
        const isFolder = entry.type !== 'file';
        const fullPath = join(state.path, entry.name);
        const confirmed = await UI.modal({
            title: I18N.t('modal.delete_title'),
            text: I18N.t('modal.delete_text') + (isFolder ? '\n' + I18N.t('modal.delete_folder') : ''),
            path: displayPath(fullPath),
            confirm: I18N.t('action.delete'),
            danger: true
        });
        if (!confirmed) return;

        try {
            await Api.remove(fullPath);
            UI.toast(I18N.t('toast.deleted', { name: entry.name }), 'success');
            if (state.viewerKey === fullPath) UI.closeViewer();
            navigate(state.path);
        } catch (error) {
            UI.toast(errorText(error), 'error');
        }
    }

    // ----- Upload -----
    function enqueueUploads(files) {
        const list = files.filter(function (file) { return file && typeof file.size === 'number'; });
        if (!list.length) return;
        const dir = state.path;
        uploadQueue = uploadQueue.then(function () {
            return list.reduce(function (chain, file) {
                return chain.then(function () { return uploadFile(file, dir); });
            }, Promise.resolve());
        });
    }

    async function uploadFile(file, dir) {
        if (file.size > MAX_UPLOAD) {
            UI.toast(I18N.t('toast.too_large', { name: file.name, size: UI.formatSize(MAX_UPLOAD) }), 'error');
            return;
        }

        const controller = new AbortController();
        const item = UI.addDockItem(file.name, 'up');
        item.onCancel(function () { controller.abort(); });
        const options = {
            signal: controller.signal,
            onProgress: function (loaded, total) {
                const percent = total > 0 ? Math.min(100, loaded * 100 / total) : 0;
                item.update(percent, percent.toFixed(0) + '% · ' + UI.formatSize(loaded) + ' / ' + UI.formatSize(total));
            }
        };

        try {
            try {
                await Api.upload(dir, file, false, options);
            } catch (error) {
                if (error && error.code === 'exists') {
                    const confirmed = await UI.modal({
                        title: I18N.t('modal.overwrite_title'),
                        text: I18N.t('modal.overwrite_text', { name: file.name }),
                        path: displayPath(join(dir, file.name)),
                        confirm: I18N.t('modal.overwrite_ok'),
                        danger: false
                    });
                    if (!confirmed) {
                        item.fail(I18N.t('err.aborted'));
                        return;
                    }
                    await Api.upload(dir, file, true, options);
                } else {
                    throw error;
                }
            }
            item.done(I18N.t('toast.uploaded', { name: file.name }));
            UI.toast(I18N.t('toast.uploaded', { name: file.name }), 'success');
            if (state.path === dir) navigate(dir);
        } catch (error) {
            if (error && (error.code === 'aborted' || error.name === 'AbortError'))
                item.fail(I18N.t('err.aborted'));
            else item.fail(errorText(error));
        }
    }

    // ----- Drag and drop -----
    function bindDragDrop() {
        let depth = 0;

        function hasFiles(event) {
            return event.dataTransfer &&
                Array.prototype.indexOf.call(event.dataTransfer.types || [], 'Files') !== -1;
        }

        document.addEventListener('dragenter', function (event) {
            if (!hasFiles(event)) return;
            event.preventDefault();
            depth++;
            if (canUploadHere()) els.dropOverlay.hidden = false;
        });
        document.addEventListener('dragover', function (event) {
            if (hasFiles(event)) event.preventDefault();
        });
        document.addEventListener('dragleave', function (event) {
            if (!hasFiles(event)) return;
            depth = Math.max(0, depth - 1);
            if (depth === 0) els.dropOverlay.hidden = true;
        });
        document.addEventListener('drop', function (event) {
            if (!hasFiles(event)) return;
            event.preventDefault();
            depth = 0;
            els.dropOverlay.hidden = true;
            if (!canUploadHere()) return;
            enqueueUploads(Array.prototype.slice.call(event.dataTransfer.files || []));
        });
    }

    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
    else boot();
})();
