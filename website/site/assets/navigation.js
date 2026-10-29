// SPDX-License-Identifier: CC-BY-NC-4.0
// Native disclosure remains usable when scripting is unavailable.
document.addEventListener('click', event => {
  for (const menu of document.querySelectorAll('.nav-guides[open]')) {
    if (!menu.contains(event.target)) menu.open = false;
  }
});
document.addEventListener('keydown', event => {
  if (event.key !== 'Escape') return;
  for (const menu of document.querySelectorAll('.nav-guides[open]')) {
    menu.open = false;
    menu.querySelector('summary').focus();
  }
});
