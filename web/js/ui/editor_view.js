// EQUORUS Studio - Editor View Component

export class EditorView {
  constructor({ container, onInputChange, onPresetChange }) {
    this.container = container;
    this.onInputChange = onInputChange;
    this.onPresetChange = onPresetChange;
    this.render();
  }

  render() {
    this.container.innerHTML = `
      <div class="flex flex-col h-full space-y-3">
        <div class="flex items-center justify-between">
          <div class="flex items-center space-x-2">
            <span class="text-sm font-semibold text-[var(--foreground)]">Preset:</span>
            <select id="preset-selector" class="text-sm bg-[var(--background)] text-[var(--foreground)] border border-[var(--border)] rounded px-2.5 py-1 focus:outline-none focus:ring-1 focus:ring-[var(--primary)]">
            </select>
          </div>
          <div class="flex items-center space-x-2">
            <button id="btn-format" class="text-xs bg-[var(--card)] hover:bg-[var(--border)] text-[var(--foreground)] border border-[var(--border)] px-2.5 py-1 rounded transition">
              Prettify
            </button>
            <button id="btn-minify" class="text-xs bg-[var(--card)] hover:bg-[var(--border)] text-[var(--foreground)] border border-[var(--border)] px-2.5 py-1 rounded transition">
              Minify
            </button>
            <button id="btn-clear" class="text-xs text-[var(--muted-foreground)] hover:text-[var(--foreground)] px-2 py-1">
              Clear
            </button>
          </div>
        </div>

        <div class="relative flex-1 min-h-[360px]">
          <textarea id="raw-json-editor" spellcheck="false"
            class="w-full h-full font-mono text-xs p-3 rounded-lg bg-[var(--background)] text-[var(--foreground)] border border-[var(--border)] focus:outline-none focus:ring-1 focus:ring-[var(--primary)] resize-none"
            placeholder="Paste or write EQUORUS JSON envelope here..."></textarea>
        </div>

        <div id="validation-banner" class="p-3 rounded-lg border text-xs font-mono transition-all">
        </div>
      </div>
    `;

    this.textarea = this.container.querySelector('#raw-json-editor');
    this.presetSelect = this.container.querySelector('#preset-selector');
    this.validationBanner = this.container.querySelector('#validation-banner');

    this.textarea.addEventListener('input', () => {
      if (this.onInputChange) this.onInputChange(this.textarea.value);
    });

    this.presetSelect.addEventListener('change', (e) => {
      if (this.onPresetChange) this.onPresetChange(e.target.value);
    });

    this.container.querySelector('#btn-format').addEventListener('click', () => {
      try {
        const parsed = JSON.parse(this.textarea.value);
        this.textarea.value = JSON.stringify(parsed, null, 2);
        if (this.onInputChange) this.onInputChange(this.textarea.value);
      } catch (e) {
        // ignore format if invalid
      }
    });

    this.container.querySelector('#btn-minify').addEventListener('click', () => {
      try {
        const parsed = JSON.parse(this.textarea.value);
        this.textarea.value = JSON.stringify(parsed);
        if (this.onInputChange) this.onInputChange(this.textarea.value);
      } catch (e) {
        // ignore
      }
    });

    this.container.querySelector('#btn-clear').addEventListener('click', () => {
      this.textarea.value = '';
      if (this.onInputChange) this.onInputChange('');
    });
  }

  setPresets(presets) {
    this.presetSelect.innerHTML = presets.map(p => `
      <option value="${p.id}">[${p.category}] ${p.name}</option>
    `).join('');
  }

  setValue(val) {
    this.textarea.value = val;
  }

  getValue() {
    return this.textarea.value;
  }

  setValidationStatus(validationResult) {
    if (validationResult.valid) {
      this.validationBanner.className = 'p-3 rounded-lg border border-emerald-500/30 bg-emerald-500/10 text-emerald-400 text-xs font-mono';
      this.validationBanner.innerHTML = `
        <div class="flex items-center space-x-2">
          <span class="inline-block w-2 h-2 rounded-full bg-emerald-400"></span>
          <span class="font-bold">EQUORUS v0.1 VALID ENVELOPE</span>
        </div>
        <div class="mt-1 text-[var(--muted-foreground)]">Type: <span class="text-[var(--foreground)]">${validationResult.typeId || 'N/A'}</span> (v${validationResult.schemaVersion || '0.1'})</div>
      `;
    } else {
      this.validationBanner.className = 'p-3 rounded-lg border border-rose-500/30 bg-rose-500/10 text-rose-400 text-xs font-mono';
      const errorList = validationResult.errors.map(err => `<li>${this._escapeHtml(err)}</li>`).join('');
      this.validationBanner.innerHTML = `
        <div class="flex items-center space-x-2">
          <span class="inline-block w-2 h-2 rounded-full bg-rose-400 animate-pulse"></span>
          <span class="font-bold">VALIDATION FAILED (${validationResult.errors.length} error${validationResult.errors.length > 1 ? 's' : ''})</span>
        </div>
        <ul class="list-disc list-inside mt-1.5 space-y-0.5 text-xs text-rose-300">
          ${errorList}
        </ul>
      `;
    }
  }

  _escapeHtml(text) {
    const div = document.createElement('div');
    div.innerText = text;
    return div.innerHTML;
  }
}
