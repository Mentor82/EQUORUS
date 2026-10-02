// EQUORUS Studio - Integrity & Tamper View Component

export class IntegrityView {
  constructor({ container, onTamperToggle }) {
    this.container = container;
    this.onTamperToggle = onTamperToggle;
    this.isTampered = false;
    this.render();
  }

  render() {
    this.container.innerHTML = `
      <div class="flex flex-col h-full space-y-4">
        <!-- Status Header Card -->
        <div id="integrity-status-card" class="p-4 rounded-xl border border-emerald-500/30 bg-emerald-500/10 flex items-center justify-between transition-all">
          <div class="flex items-center space-x-3">
            <div id="integrity-status-icon" class="w-3 h-3 rounded-full bg-emerald-400"></div>
            <div>
              <div id="integrity-status-title" class="font-bold text-sm text-emerald-400">INTEGRITY VERIFIED</div>
              <div id="integrity-status-desc" class="text-xs text-[var(--muted-foreground)]">Matches domain-separated SHA-256 digest</div>
            </div>
          </div>
          <div>
            <button id="btn-tamper-toggle" class="text-xs font-semibold px-3.5 py-1.5 rounded-lg border border-amber-500/40 bg-amber-500/10 hover:bg-amber-500/20 text-amber-300 transition shadow-xs cursor-pointer active:scale-95 flex items-center space-x-1.5">
              <span>⚠️ Simulate Tampering (Bit-Flip)</span>
            </button>
          </div>
        </div>

        <!-- Digest display -->
        <div class="space-y-1.5">
          <div class="flex items-center justify-between">
            <span class="text-xs font-semibold text-[var(--foreground)] uppercase tracking-wider">Detached SHA-256 Digest:</span>
            <button id="btn-copy-digest" class="text-xs font-medium bg-[var(--secondary)] hover:bg-[var(--border)] text-[var(--foreground)] border border-[var(--border)] px-2.5 py-1 rounded-md transition shadow-xs cursor-pointer active:scale-95">
              Copy Digest
            </button>
          </div>
          <div id="integrity-digest" class="font-mono text-sm p-3 rounded-lg bg-[var(--background)] text-cyan-300 border border-[var(--border)] select-all break-all font-bold">
            --
          </div>
        </div>

        <!-- Preimage Formula Breakdown -->
        <div class="space-y-1.5">
          <span class="text-xs font-semibold text-[var(--muted-foreground)] uppercase tracking-wider">Domain-Separated Preimage Contract:</span>
          <div class="p-3 rounded-lg bg-[var(--background)] border border-[var(--border)] text-xs font-mono space-y-1.5">
            <div class="text-[var(--muted-foreground)] flex items-center space-x-1">
              <span class="text-purple-400">ASCII("EQUORUS-INTEGRITY")</span>
              <span>|| 0x00 ||</span>
              <span class="text-purple-400">ASCII("v1")</span>
              <span>|| 0x00 ||</span>
            </div>
            <div class="text-[var(--muted-foreground)] flex items-center space-x-1">
              <span class="text-blue-400">ASCII("equorus-value-v1")</span>
              <span>|| 0x00 ||</span>
              <span class="text-emerald-400">ASCII("sha-256")</span>
              <span>|| 0x00 ||</span>
            </div>
            <div class="text-amber-400 font-semibold">
              || canonical_bytes(envelope) [<span id="preimage-canon-len">0</span> bytes]
            </div>
          </div>
        </div>

        <!-- Detached JSON Record -->
        <div class="flex-1 flex flex-col space-y-1.5 min-h-[140px]">
          <span class="text-xs font-semibold text-[var(--muted-foreground)] uppercase tracking-wider">Detached Integrity Record (JSON):</span>
          <pre id="detached-record-json" class="flex-1 font-mono text-xs p-3 rounded-lg bg-[var(--background)] text-[var(--foreground)] border border-[var(--border)] select-all overflow-auto">--</pre>
        </div>
      </div>
    `;

    this.statusCard = this.container.querySelector('#integrity-status-card');
    this.statusIcon = this.container.querySelector('#integrity-status-icon');
    this.statusTitle = this.container.querySelector('#integrity-status-title');
    this.statusDesc = this.container.querySelector('#integrity-status-desc');
    this.digestEl = this.container.querySelector('#integrity-digest');
    this.recordJsonEl = this.container.querySelector('#detached-record-json');
    this.preimageLenEl = this.container.querySelector('#preimage-canon-len');
    this.tamperBtn = this.container.querySelector('#btn-tamper-toggle');
    this.copyDigestBtn = this.container.querySelector('#btn-copy-digest');

    this.tamperBtn.addEventListener('click', () => {
      this.isTampered = !this.isTampered;
      this.tamperBtn.innerText = this.isTampered ? 'Restore Untampered' : 'Simulate Tampering (Bit-Flip)';
      if (this.onTamperToggle) {
        this.onTamperToggle(this.isTampered);
      }
    });

    this.copyDigestBtn.addEventListener('click', () => {
      const text = this.digestEl.innerText;
      if (text && text !== '--') {
        navigator.clipboard.writeText(text);
        const orig = this.copyDigestBtn.innerText;
        this.copyDigestBtn.innerText = 'Copied!';
        setTimeout(() => this.copyDigestBtn.innerText = orig, 1500);
      }
    });
  }

  update({ record, canonicalBytes, isValid, isTampered = false }) {
    this.isTampered = isTampered;
    this.tamperBtn.innerText = isTampered ? 'Restore Untampered' : 'Simulate Tampering (Bit-Flip)';

    if (!isValid || !record) {
      this.statusCard.className = 'p-4 rounded-xl border border-rose-500/30 bg-rose-500/10 flex items-center justify-between';
      this.statusIcon.className = 'w-3 h-3 rounded-full bg-rose-400';
      this.statusTitle.innerText = 'INTEGRITY UNAVAILABLE';
      this.statusTitle.className = 'font-bold text-sm text-rose-400';
      this.statusDesc.innerText = 'Input does not form a valid EQUORUS envelope';
      this.digestEl.innerText = '--';
      this.recordJsonEl.innerText = '--';
      this.preimageLenEl.innerText = '0';
      return;
    }

    if (isTampered) {
      this.statusCard.className = 'p-4 rounded-xl border border-rose-500/50 bg-rose-500/15 flex items-center justify-between';
      this.statusIcon.className = 'w-3 h-3 rounded-full bg-rose-400 animate-ping';
      this.statusTitle.innerText = 'TAMPER DETECTED: MISMATCH';
      this.statusTitle.className = 'font-bold text-sm text-rose-400';
      this.statusDesc.innerText = 'Canonical payload altered! Signature/record rejected without silent coercion.';
    } else {
      this.statusCard.className = 'p-4 rounded-xl border border-emerald-500/30 bg-emerald-500/10 flex items-center justify-between';
      this.statusIcon.className = 'w-3 h-3 rounded-full bg-emerald-400';
      this.statusTitle.innerText = 'INTEGRITY VERIFIED (PASS)';
      this.statusTitle.className = 'font-bold text-sm text-emerald-400';
      this.statusDesc.innerText = 'Digest matches exact canonical equorus-value-v1 preimage';
    }

    this.digestEl.innerText = record.digest;
    this.preimageLenEl.innerText = canonicalBytes ? canonicalBytes.length.toString() : '0';
    this.recordJsonEl.innerText = JSON.stringify(record, null, 2);
  }
}
