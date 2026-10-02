// EQUORUS Studio - Canonical Byte & Hex Diff View

import { bytesToHex, formatHexDump } from '../core/canonical.js';

export class DiffView {
  constructor({ container }) {
    this.container = container;
    this.render();
  }

  render() {
    this.container.innerHTML = `
      <div class="flex flex-col h-full space-y-3">
        <div class="flex items-center justify-between">
          <div class="flex items-center space-x-2">
            <span class="text-xs font-semibold text-[var(--foreground)] uppercase tracking-wider">Profile:</span>
            <span class="text-xs font-mono bg-blue-500/10 text-blue-400 px-2 py-0.5 rounded border border-blue-500/20">equorus-value-v1</span>
          </div>
          <div id="byte-stats" class="text-xs font-mono text-[var(--muted-foreground)]">
            Canonical: 0 B | Raw: 0 B
          </div>
        </div>

        <div class="flex flex-col space-y-2">
          <div class="text-xs font-semibold text-[var(--muted-foreground)]">Canonical Hex Stream:</div>
          <div class="relative">
            <div id="canonical-hex-raw" class="font-mono text-xs p-2.5 rounded bg-[var(--background)] text-amber-300/90 border border-[var(--border)] overflow-x-auto break-all select-all max-h-24">
              --
            </div>
          </div>
        </div>

        <div class="flex-1 flex flex-col space-y-2 min-h-[220px]">
          <div class="flex items-center justify-between">
            <span class="text-xs font-semibold text-[var(--muted-foreground)]">Canonical Hex Dump & ASCII representation:</span>
            <button id="btn-copy-hex" class="text-xs font-medium bg-[var(--secondary)] hover:bg-[var(--border)] text-[var(--foreground)] border border-[var(--border)] px-2.5 py-1 rounded-md transition shadow-xs cursor-pointer active:scale-95">
              Copy Hex
            </button>
          </div>
          <pre id="canonical-hex-dump" class="flex-1 font-mono text-xs p-3 rounded bg-[var(--background)] text-emerald-400/90 border border-[var(--border)] overflow-auto select-all leading-relaxed whitespace-pre">--</pre>
        </div>
      </div>
    `;

    this.hexRawEl = this.container.querySelector('#canonical-hex-raw');
    this.hexDumpEl = this.container.querySelector('#canonical-hex-dump');
    this.byteStatsEl = this.container.querySelector('#byte-stats');
    this.copyBtn = this.container.querySelector('#btn-copy-hex');

    this.copyBtn.addEventListener('click', () => {
      const text = this.hexRawEl.innerText;
      if (text && text !== '--') {
        navigator.clipboard.writeText(text);
        const orig = this.copyBtn.innerText;
        this.copyBtn.innerText = 'Copied!';
        setTimeout(() => this.copyBtn.innerText = orig, 1500);
      }
    });
  }

  update({ canonicalBytes, rawText }) {
    if (!canonicalBytes || canonicalBytes.length === 0) {
      this.hexRawEl.innerText = '-- (Invalid or empty envelope)';
      this.hexDumpEl.innerText = '--';
      this.byteStatsEl.innerText = 'Canonical: 0 B | Raw: 0 B';
      return;
    }

    const hex = bytesToHex(canonicalBytes);
    this.hexRawEl.innerText = hex;
    this.hexDumpEl.innerText = formatHexDump(canonicalBytes, 16);

    const rawBytesLen = new TextEncoder().encode(rawText || '').length;
    this.byteStatsEl.innerHTML = `
      <span class="text-[var(--foreground)] font-semibold">${canonicalBytes.length} B</span> canonical | 
      <span>${rawBytesLen} B</span> raw JSON
    `;
  }
}
