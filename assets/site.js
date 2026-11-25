// Progressively enhance the static comparisons and language links.
for (const menu of document.querySelectorAll('[data-language]')) {
  const current = [...menu.options].find((option) => option.defaultSelected).value;
  window.addEventListener('pageshow', () => { menu.value = current; });
  menu.value = current;
  menu.addEventListener('change', () => window.location.assign(menu.value));
  menu.closest('.language-select').hidden = false;
  menu.closest('.languages').querySelector('.language-fallback').hidden = true;
}

for (const control of document.querySelectorAll('.comparison-control')) {
  const comparison = control.closest('.image-comparison');
  const update = () => comparison.style.setProperty('--split', `${control.value}%`);
  control.addEventListener('input', update);
  control.hidden = false;
  comparison.querySelector('.comparison-knob').hidden = false;
  update();
}

for (const preview of document.querySelectorAll('[data-preview]')) {
  const options = preview.querySelector('[data-preview-options]');
  const image = preview.querySelector('[data-preview-image]');
  const labels = preview.querySelectorAll('[data-preview-label]');
  const buttons = [...options.querySelectorAll('button')];

  options.addEventListener('click', (event) => {
    const button = event.target.closest('button[data-image]');
    if (!button || button.getAttribute('aria-pressed') === 'true') return;

    const { image: src, srcset, name } = button.dataset;
    if (srcset) image.srcset = srcset;
    image.src = src;
    image.alt = `${preview.dataset.alt} — ${name}`;
    for (const label of labels) label.textContent = name;
    for (const option of buttons) option.setAttribute('aria-pressed', String(option === button));
  });
  options.hidden = false;
}
