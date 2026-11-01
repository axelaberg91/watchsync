// SPDX-License-Identifier: CC-BY-NC-4.0
// Keep existing printed QR codes and installed firmware help links usable.
export default {
  async fetch(request) {
    if (request.method !== 'GET' && request.method !== 'HEAD') {
      return new Response('Method not allowed', { status: 405, headers: { Allow: 'GET, HEAD' } });
    }
    const source = new URL(request.url);
    const destination = new URL('https://axelaberg91.github.io/watchsync/');
    let path = source.pathname.replace(/^\/watchsync(?:\/|$)/, '/').replace(/^\/+/, '');
    // The previous site also accepted extensionless page names.
    if (path && !path.endsWith('/') && !path.split('/').at(-1).includes('.')) path += '.html';
    destination.pathname += path;
    destination.search = source.search;
    return Response.redirect(destination.href, 308);
  },
};
