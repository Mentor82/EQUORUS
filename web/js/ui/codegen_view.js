// EQUORUS Studio - Polyglot Code Generator View

import { PolyglotGenerator } from '../codegen/polyglot.js';

export class CodegenView {
  constructor({ container }) {
    this.container = container;
    this.currentLanguage = 'cpp';
    this.snippets = { cpp: '', c_abi: '', python: '', go: '', rust: '' };
    this.render();
  }

  render() {
    this.container.innerHTML = `
      <div class="flex flex-col h-full space-y-3">
        <div class="flex items-center justify-between border-b border-[var(--border)] pb-2">
          <div class="flex space-x-1" id="lang-tabs">
            <button data-lang="cpp" class="text-xs px-3 py-1 rounded-md font-semibold transition bg-[var(--primary)] text-[var(--primary-foreground)]">C++20</button>
            <button data-lang="c_abi" class="text-xs px-3 py-1 rounded-md font-semibold transition text-[var(--muted-foreground)] hover:text-[var(--foreground)]">C ABI</button>
            <button data-lang="python" class="text-xs px-3 py-1 rounded-md font-semibold transition text-[var(--muted-foreground)] hover:text-[var(--foreground)]">Python</button>
            <button data-lang="go" class="text-xs px-3 py-1 rounded-md font-semibold transition text-[var(--muted-foreground)] hover:text-[var(--foreground)]">Go</button>
            <button data-lang="rust" class="text-xs px-3 py-1 rounded-md font-semibold transition text-[var(--muted-foreground)] hover:text-[var(--foreground)]">Rust (std)</button>
          </div>
          <div>
            <button id="btn-copy-code" class="text-xs text-[var(--muted-foreground)] hover:text-[var(--foreground)] border border-[var(--border)] px-2.5 py-1 rounded transition">
              Copy Snippet
            </button>
          </div>
        </div>

        <div class="relative flex-1 min-h-[300px]">
          <pre id="code-snippet-pre" class="w-full h-full font-mono text-xs p-3.5 rounded-lg bg-[var(--background)] text-cyan-200/90 border border-[var(--border)] overflow-auto select-all leading-relaxed whitespace-pre"></pre>
        </div>
      </div>
    `;

    this.codePre = this.container.querySelector('#code-snippet-pre');
    this.copyBtn = this.container.querySelector('#btn-copy-code');
    this.tabsContainer = this.container.querySelector('#lang-tabs');

    this.tabsContainer.addEventListener('click', (e) => {
      const btn = e.target.closest('button[data-lang]');
      if (!btn) return;
      const lang = btn.dataset.lang;
      this._selectTab(lang);
    });

    this.copyBtn.addEventListener('click', () => {
      const code = this.snippets[this.currentLanguage];
      if (code) {
        navigator.clipboard.writeText(code);
        const orig = this.copyBtn.innerText;
        this.copyBtn.innerText = 'Copied!';
        setTimeout(() => this.copyBtn.innerText = orig, 1500);
      }
    });
  }

  _selectTab(lang) {
    this.currentLanguage = lang;
    const buttons = this.tabsContainer.querySelectorAll('button[data-lang]');
    buttons.forEach(b => {
      if (b.dataset.lang === lang) {
        b.className = 'text-xs px-3 py-1 rounded-md font-semibold transition bg-[var(--primary)] text-[var(--primary-foreground)]';
      } else {
        b.className = 'text-xs px-3 py-1 rounded-md font-semibold transition text-[var(--muted-foreground)] hover:text-[var(--foreground)]';
      }
    });
    this.codePre.innerText = this.snippets[lang] || '// No code available';
  }

  update({ rawJson, typeId, isValid }) {
    if (!isValid || !rawJson) {
      this.snippets = {
        cpp: '// Valid EQUORUS envelope required for code generation',
        c_abi: '/* Valid EQUORUS envelope required for code generation */',
        python: '# Valid EQUORUS envelope required for code generation',
        go: '// Valid EQUORUS envelope required for code generation',
        rust: '// Valid EQUORUS envelope required for code generation'
      };
    } else {
      try {
        this.snippets = PolyglotGenerator.generateAll(rawJson, typeId);
      } catch (e) {
        this.snippets = { cpp: `// Error: ${e.message}`, c_abi: '', python: '', go: '', rust: '' };
      }
    }
    this.codePre.innerText = this.snippets[this.currentLanguage] || '';
  }
}
