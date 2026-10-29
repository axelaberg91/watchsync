// SPDX-License-Identifier: CC-BY-NC-4.0
// Choose a first-visit language, then respect the visitor's explicit choice.
(() => {
  'use strict';
  const key = 'watchsync-language';
  const current = document.documentElement.lang;
  const page = document.documentElement.dataset.page;
  const url = new URL(location.href);
  const explicit = url.searchParams.get('lang');
  const valid = value => value === 'en' || value === 'sv';
  let saved;
  try { saved = localStorage.getItem(key); } catch { /* Storage may be disabled. */ }
  if (valid(explicit)) {
    saved = explicit;
    try { localStorage.setItem(key, explicit); } catch { /* Links still work. */ }
  }
  const browserLanguage = navigator.languages?.[0] || navigator.language || 'en';
  // A direct Swedish URL is already an explicit language selection.
  const initial = current === 'sv' ? 'sv' : /^sv(?:-|$)/i.test(browserLanguage) ? 'sv' : 'en';
  const desired = valid(saved) ? saved : initial;
  if (page && page !== '404.html' && desired !== current) {
    const target = new URL(desired === 'sv' ? '/sv/' + page : '/' + page, url);
    target.search = url.search;
    target.hash = url.hash;
    location.replace(target.href);
    return;
  }
  document.addEventListener('DOMContentLoaded', () => {
    for (const link of document.querySelectorAll('[data-language]')) {
      const target = new URL(link.href);
      target.search = url.search;
      target.searchParams.set('lang', link.dataset.language);
      target.hash = url.hash;
      link.href = target.href;
    }
    // Keep explicit choices working during navigation even if storage is blocked.
    if (valid(explicit)) {
      for (const link of document.querySelectorAll('a[href]')) {
        const target = new URL(link.href);
        if (target.origin === url.origin && target.pathname.endsWith('.html') && !link.dataset.language) {
          target.searchParams.set('lang', current);
          link.href = target.href;
        }
      }
    }
  });
})();
