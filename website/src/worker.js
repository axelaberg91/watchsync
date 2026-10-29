// SPDX-License-Identifier: CC-BY-NC-4.0
// Original bounded static website handler by axelaberg91.
// Replaced at build time; only the audited site assets are served at runtime.
const files = __WATCHSYNC_ASSETS__;
export default {
  async fetch(request) {
    if (request.method !== 'GET' && request.method !== 'HEAD') {
      return new Response('Method not allowed', {status:405,headers:{Allow:'GET, HEAD'}});
    }
    const url = new URL(request.url);
    let path = url.pathname.endsWith('/') ? url.pathname + 'index.html' : url.pathname;
    if (!files[path] && !path.split('/').at(-1).includes('.')) path += '.html';
    const found = files[path];
    const file = found || files[path.startsWith('/sv/') ? '/sv/404.html' : '/404.html'];
    const headers = new Headers({
      'Content-Type':file.mime,
      'Cache-Control':'public, max-age=300',
      'ETag':'"'+file.hash+'"',
      'X-Content-Type-Options':'nosniff',
      'Referrer-Policy':'strict-origin-when-cross-origin'
    });
    if (found && request.headers.get('If-None-Match') === headers.get('ETag')) {
      return new Response(null,{status:304,headers});
    }
    const body = request.method === 'HEAD' ? null : Uint8Array.from(atob(file.base64),c=>c.charCodeAt(0));
    return new Response(body,{status:found?200:404,headers});
  }
};
