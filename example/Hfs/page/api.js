var Api = (function () {
    'use strict';

    function ApiError(code, status) {
        this.name = 'ApiError';
        this.code = code || 'unknown';
        this.status = status || 0;
        this.message = this.code;
    }
    ApiError.prototype = Object.create(Error.prototype);
    ApiError.prototype.constructor = ApiError;

    async function errorOf(response) {
        var code = 'unknown';
        try {
            var body = await response.json();
            if (body && body.error) code = body.error;
        } catch (e) { }
        return new ApiError(code, response.status);
    }

    async function root() {
        const response = await fetch('/filesys/root');
        if (!response.ok) throw await errorOf(response);
        return response.json();
    }

    async function list(path) {
        const response = await fetch('/filesys/list?path=' + encodeURIComponent(path));
        if (!response.ok) throw await errorOf(response);
        return response.json();
    }

    async function readBlock(path, index, signal) {
        const response = await fetch('/filesys/read?path=' + encodeURIComponent(path), {
            headers: { 'FileBlockIndex': String(index) },
            signal: signal
        });
        if (!response.ok) throw await errorOf(response);
        const data = await response.arrayBuffer();
        const got = parseInt(response.headers.get('FileBlockIndex'), 10);
        const total = parseInt(response.headers.get('FileBlockTotal'), 10);
        return {
            data: data,
            index: isNaN(got) ? index : got,
            total: isNaN(total) || total < 1 ? 1 : total
        };
    }

    function upload(path, file, overwrite, options) {
        options = options || {};
        return new Promise(function (resolve, reject) {
            const xhr = new XMLHttpRequest();
            const query = '/filesys/upload?path=' + encodeURIComponent(path) +
                '&name=' + encodeURIComponent(file.name) +
                (overwrite ? '&overwrite=1' : '');
            xhr.open('POST', query);
            xhr.setRequestHeader('Content-Type', 'application/octet-stream');
            xhr.upload.onprogress = function (event) {
                if (event.lengthComputable && options.onProgress)
                    options.onProgress(event.loaded, event.total);
            };
            xhr.onload = function () {
                if (xhr.status >= 200 && xhr.status < 300) {
                    try { resolve(JSON.parse(xhr.responseText || '{}')); }
                    catch (e) { resolve({}); }
                    return;
                }
                let code = 'unknown';
                try {
                    const body = JSON.parse(xhr.responseText);
                    if (body && body.error) code = body.error;
                } catch (e) { }
                reject(new ApiError(code, xhr.status));
            };
            xhr.onerror = function () { reject(new ApiError('network', 0)); };
            xhr.onabort = function () { reject(new ApiError('aborted', 0)); };
            if (options.signal) {
                if (options.signal.aborted) {
                    xhr.abort();
                    return;
                }
                options.signal.addEventListener('abort', function () { xhr.abort(); });
            }
            xhr.send(file);
        });
    }

    async function remove(path) {
        const response = await fetch('/filesys/remove?path=' + encodeURIComponent(path), { method: 'DELETE' });
        if (!response.ok) throw await errorOf(response);
        return response.json();
    }

    return {
        root: root,
        list: list,
        readBlock: readBlock,
        upload: upload,
        remove: remove,
        ApiError: ApiError
    };
})();
