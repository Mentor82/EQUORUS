// EQUORUS Studio - Live Multi-Runtime Conformance & Cluster Compare View

export class CompareView {
  constructor({ container, onExecuteCompare }) {
    this.container = container;
    this.onExecuteCompare = onExecuteCompare;
    
    // Auto-detect daemon endpoint
    const isRemoteDevHost = window.location.hostname === '192.168.178.161' || window.location.port === '8088';
    this.daemonUrl = isRemoteDevHost ? '' : 'http://192.168.178.161:8088';
    
    this.daemonStatus = null;
    this.latestComparison = null;
    this.currentEnvelope = null;
    this.currentRaw = '';
    this.currentTypeId = 'vinox.provenance.snapshot';
    this.browserDigest = null;
    this.autoCompare = true;

    this.render();
    this.checkDaemonHealth();
  }

  render() {
    this.container.innerHTML = `
      <div class="flex flex-col h-full space-y-4">
        <!-- Top Toolbar & Cluster Node Endpoint -->
        <div class="bg-[var(--background)] p-3 rounded-lg border border-[var(--border)] flex flex-wrap items-center justify-between gap-3 text-xs">
          <div class="flex items-center space-x-2 flex-1 min-w-[280px]">
            <span class="font-semibold text-[var(--muted-foreground)] whitespace-nowrap">Daemon Node:</span>
            <input id="daemon-endpoint-input" type="text" value="${this.daemonUrl}" 
              placeholder="http://192.168.178.161:8088"
              class="flex-1 bg-[var(--card)] text-[var(--foreground)] border border-[var(--border)] rounded px-2 py-1 font-mono text-xs focus:outline-none focus:border-[var(--primary)]" />
            <button id="btn-check-daemon" class="px-2.5 py-1 rounded bg-[var(--secondary)] hover:bg-[var(--border)] text-[var(--foreground)] border border-[var(--border)] font-medium transition cursor-pointer active:scale-95">
              Connect
            </button>
          </div>

          <div class="flex items-center space-x-3">
            <label class="flex items-center space-x-1.5 cursor-pointer select-none text-[var(--muted-foreground)] hover:text-[var(--foreground)]">
              <input id="cb-auto-compare" type="checkbox" ${this.autoCompare ? 'checked' : ''} class="rounded border-[var(--border)] text-[var(--primary)] focus:ring-0 cursor-pointer">
              <span>Auto-Run</span>
            </label>
            <button id="btn-run-compare" class="px-3 py-1 rounded bg-[var(--primary)] hover:bg-[var(--primary-hover)] text-white font-semibold transition flex items-center space-x-1.5 cursor-pointer shadow-xs active:scale-95">
              <svg class="w-3.5 h-3.5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z"></path><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>
              <span>Run Compare</span>
            </button>
          </div>
        </div>

        <!-- Node Health & Active Runtimes Badge -->
        <div id="node-health-bar" class="flex items-center justify-between text-xs px-3 py-2 rounded-lg bg-[var(--card)] border border-[var(--border)]">
          <div class="flex items-center space-x-2">
            <span class="w-2 h-2 rounded-full bg-amber-400 animate-pulse" id="node-status-dot"></span>
            <span class="font-medium text-[var(--foreground)]" id="node-status-label">Probing L.I.A.R.A. OS Dev Daemon...</span>
          </div>
          <div id="node-runtimes-pills" class="flex items-center space-x-1.5 font-mono text-[11px]">
            <span class="px-2 py-0.5 rounded bg-[var(--background)] border border-[var(--border)] text-[var(--muted-foreground)]">C++20</span>
            <span class="px-2 py-0.5 rounded bg-[var(--background)] border border-[var(--border)] text-[var(--muted-foreground)]">Rust</span>
            <span class="px-2 py-0.5 rounded bg-[var(--background)] border border-[var(--border)] text-[var(--muted-foreground)]">Go</span>
            <span class="px-2 py-0.5 rounded bg-[var(--background)] border border-[var(--border)] text-[var(--muted-foreground)]">Python</span>
            <span class="px-2 py-0.5 rounded bg-[var(--background)] border border-[var(--border)] text-emerald-400">Browser JS</span>
          </div>
        </div>

        <!-- Consensus Banner -->
        <div id="consensus-banner" class="p-3.5 rounded-xl border border-[var(--border)] bg-[var(--background)] flex items-center justify-between">
          <div class="flex items-center space-x-3">
            <div id="consensus-icon" class="w-7 h-7 rounded-lg bg-[var(--secondary)] flex items-center justify-center font-bold text-xs text-[var(--muted-foreground)]">
              --
            </div>
            <div>
              <div id="consensus-title" class="text-xs font-bold text-[var(--foreground)]">Awaiting Verification</div>
              <div id="consensus-desc" class="text-[11px] font-mono text-[var(--muted-foreground)] break-all">Run comparison across 5 independent conformance engines</div>
            </div>
          </div>
          <div id="consensus-stats" class="text-right text-xs font-mono text-[var(--muted-foreground)] hidden sm:block">
            Latency &bull; Conformance
          </div>
        </div>

        <!-- Multi-Runtime Matrix Grid -->
        <div class="flex-1 flex flex-col space-y-2">
          <div class="text-xs font-semibold text-[var(--muted-foreground)] uppercase tracking-wider flex items-center justify-between">
            <span>Runtime Conformance Matrix (5 Engines)</span>
            <span class="text-[11px] text-[var(--muted-foreground)] font-normal">Canonical Envelope SHA-256</span>
          </div>

          <div id="runtime-grid" class="grid grid-cols-1 gap-2.5 overflow-y-auto max-h-[380px] pr-1">
            <!-- Rendered dynamically -->
          </div>
        </div>
      </div>
    `;

    // Elements
    this.endpointInput = this.container.querySelector('#daemon-endpoint-input');
    this.btnCheckDaemon = this.container.querySelector('#btn-check-daemon');
    this.btnRunCompare = this.container.querySelector('#btn-run-compare');
    this.cbAutoCompare = this.container.querySelector('#cb-auto-compare');
    this.statusDot = this.container.querySelector('#node-status-dot');
    this.statusLabel = this.container.querySelector('#node-status-label');
    this.runtimesPills = this.container.querySelector('#node-runtimes-pills');
    this.consensusBanner = this.container.querySelector('#consensus-banner');
    this.consensusIcon = this.container.querySelector('#consensus-icon');
    this.consensusTitle = this.container.querySelector('#consensus-title');
    this.consensusDesc = this.container.querySelector('#consensus-desc');
    this.runtimeGrid = this.container.querySelector('#runtime-grid');

    // Events
    this.btnCheckDaemon.addEventListener('click', () => {
      this.daemonUrl = this.endpointInput.value.trim().replace(/\/$/, '');
      this.checkDaemonHealth();
    });

    this.btnRunCompare.addEventListener('click', () => {
      this.executeCompare();
    });

    this.cbAutoCompare.addEventListener('change', (e) => {
      this.autoCompare = e.target.checked;
    });

    this._renderEmptyGrid();
  }

  async checkDaemonHealth() {
    this.statusDot.className = 'w-2 h-2 rounded-full bg-amber-400 animate-pulse';
    this.statusLabel.innerText = 'Connecting to daemon...';

    const baseUrl = this.daemonUrl || '';
    try {
      const res = await fetch(`${baseUrl}/api/status`, { mode: 'cors' });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      this.daemonStatus = data;

      this.statusDot.className = 'w-2 h-2 rounded-full bg-emerald-400';
      this.statusLabel.innerHTML = `<span class="text-emerald-400 font-semibold">Online:</span> ${data.server} (${data.os.toUpperCase()})`;

      const r = data.runtimes;
      this.runtimesPills.innerHTML = `
        <span class="px-2 py-0.5 rounded ${r.cpp ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/20' : 'bg-red-500/10 text-red-400 border border-red-500/20'}">C++20</span>
        <span class="px-2 py-0.5 rounded ${r.rust ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/20' : 'bg-red-500/10 text-red-400 border border-red-500/20'}">Rust</span>
        <span class="px-2 py-0.5 rounded ${r.go ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/20' : 'bg-red-500/10 text-red-400 border border-red-500/20'}">Go</span>
        <span class="px-2 py-0.5 rounded ${r.python ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/20' : 'bg-amber-500/10 text-amber-400 border border-amber-500/20'}">Python</span>
        <span class="px-2 py-0.5 rounded bg-emerald-500/10 text-emerald-400 border border-emerald-500/20">Browser JS</span>
      `;

      if (this.currentRaw && this.autoCompare) {
        this.executeCompare();
      }
    } catch (e) {
      this.daemonStatus = null;
      this.statusDot.className = 'w-2 h-2 rounded-full bg-red-400';
      this.statusLabel.innerHTML = `<span class="text-red-400 font-semibold">Offline:</span> Could not reach daemon at ${this.daemonUrl || window.location.origin}`;
      this.runtimesPills.innerHTML = `
        <span class="px-2 py-0.5 rounded bg-[var(--background)] border border-[var(--border)] text-[var(--muted-foreground)]">C++20</span>
        <span class="px-2 py-0.5 rounded bg-[var(--background)] border border-[var(--border)] text-[var(--muted-foreground)]">Rust</span>
        <span class="px-2 py-0.5 rounded bg-[var(--background)] border border-[var(--border)] text-[var(--muted-foreground)]">Go</span>
        <span class="px-2 py-0.5 rounded bg-[var(--background)] border border-[var(--border)] text-[var(--muted-foreground)]">Python</span>
        <span class="px-2 py-0.5 rounded bg-emerald-500/10 text-emerald-400 border border-emerald-500/20">Browser JS</span>
      `;
    }
  }

  update({ rawJson, typeId, browserDigest, isValid }) {
    this.currentRaw = rawJson;
    this.currentTypeId = typeId || 'vinox.provenance.snapshot';
    this.browserDigest = browserDigest;

    if (!isValid || !rawJson) {
      this._renderEmptyGrid();
      return;
    }

    if (this.autoCompare && this.daemonStatus) {
      this.executeCompare();
    } else {
      this._renderBrowserOnly();
    }
  }

  async executeCompare() {
    if (!this.currentRaw) return;

    const t0 = performance.now();
    let browserTimeUs = 0;
    if (this.browserDigest) {
      browserTimeUs = Math.round((performance.now() - t0) * 1000);
    }

    const baseUrl = this.daemonUrl || '';
    let remoteResults = {};

    try {
      const res = await fetch(`${baseUrl}/api/compare`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          raw: this.currentRaw,
          type_id: this.currentTypeId
        })
      });

      if (!res.ok) {
        throw new Error(`HTTP ${res.status}`);
      }
      remoteResults = await res.json();
    } catch (e) {
      remoteResults = {
        results: {
          python: { status: 'OFFLINE', error: e.message },
          cpp: { status: 'OFFLINE', error: e.message },
          rust: { status: 'OFFLINE', error: e.message },
          go: { status: 'OFFLINE', error: e.message }
        },
        agreement: false
      };
    }

    // Combine with browser JS engine
    const combined = {
      browser: {
        name: 'Browser JS (WebCrypto)',
        tech: 'SubtleCrypto SHA-256',
        status: this.browserDigest ? 'OK' : 'ERROR',
        digest: this.browserDigest,
        micros: browserTimeUs
      },
      cpp: {
        name: 'C++20 Native Engine',
        tech: 'GCC 16.2.0 &bull; glibc 2.44',
        ...(remoteResults.results?.cpp || { status: 'UNAVAILABLE' })
      },
      rust: {
        name: 'Rust Conformance Binary',
        tech: 'Rust 1.85.0 &bull; 100% Rust stdlib',
        ...(remoteResults.results?.rust || { status: 'UNAVAILABLE' })
      },
      go: {
        name: 'Go Conformance Binary',
        tech: 'Go 1.24.1 &bull; crypto/sha256',
        ...(remoteResults.results?.go || { status: 'UNAVAILABLE' })
      },
      python: {
        name: 'Python Reference Core',
        tech: 'Python 3.14.7 &bull; hashlib',
        ...(remoteResults.results?.python || { status: 'UNAVAILABLE' })
      }
    };

    this._renderMatrix(combined);
  }

  _renderMatrix(runtimes) {
    const list = Object.values(runtimes);
    const validDigests = list.filter(r => r.status === 'OK' && r.digest).map(r => r.digest);
    const unanimous = validDigests.length >= 2 && validDigests.every(d => d === validDigests[0]);
    const referenceDigest = validDigests[0] || '--';

    // Update Consensus Banner
    if (unanimous && validDigests.length >= 4) {
      this.consensusBanner.className = 'p-3.5 rounded-xl border border-emerald-500/30 bg-emerald-500/10 flex items-center justify-between';
      this.consensusIcon.className = 'w-7 h-7 rounded-lg bg-emerald-500 text-white flex items-center justify-center font-bold text-xs shadow-xs';
      this.consensusIcon.innerHTML = '✓';
      this.consensusTitle.innerHTML = `<span class="text-emerald-400 font-bold">${validDigests.length}/${list.length} Engines in 100% Unanimous Agreement</span>`;
      this.consensusDesc.innerHTML = `<span class="font-mono text-emerald-300 select-all">${referenceDigest}</span>`;
    } else if (unanimous) {
      this.consensusBanner.className = 'p-3.5 rounded-xl border border-blue-500/30 bg-blue-500/10 flex items-center justify-between';
      this.consensusIcon.className = 'w-7 h-7 rounded-lg bg-blue-500 text-white flex items-center justify-center font-bold text-xs shadow-xs';
      this.consensusIcon.innerHTML = '✓';
      this.consensusTitle.innerHTML = `<span class="text-blue-400 font-bold">${validDigests.length} Engines Matching</span>`;
      this.consensusDesc.innerHTML = `<span class="font-mono text-blue-300 select-all">${referenceDigest}</span>`;
    } else if (validDigests.length > 0 && !unanimous) {
      this.consensusBanner.className = 'p-3.5 rounded-xl border border-red-500/30 bg-red-500/10 flex items-center justify-between';
      this.consensusIcon.className = 'w-7 h-7 rounded-lg bg-red-500 text-white flex items-center justify-center font-bold text-xs shadow-xs';
      this.consensusIcon.innerHTML = '✗';
      this.consensusTitle.innerHTML = `<span class="text-red-400 font-bold">CONFORMANCE DRIFT / MISMATCH DETECTED</span>`;
      this.consensusDesc.innerHTML = `<span class="font-mono text-red-300">Different runtimes produced conflicting canonical digests!</span>`;
    } else {
      this.consensusBanner.className = 'p-3.5 rounded-xl border border-[var(--border)] bg-[var(--background)] flex items-center justify-between';
      this.consensusIcon.className = 'w-7 h-7 rounded-lg bg-[var(--secondary)] flex items-center justify-center font-bold text-xs text-[var(--muted-foreground)]';
      this.consensusIcon.innerHTML = '--';
      this.consensusTitle.innerText = 'No Run Complete';
      this.consensusDesc.innerText = 'Click "Run Compare" or enable Auto-Run.';
    }

    // Render Matrix Cards
    this.runtimeGrid.innerHTML = Object.entries(runtimes).map(([key, r]) => {
      const isOk = r.status === 'OK';
      const isMatch = isOk && r.digest === referenceDigest;
      const statusBadge = isOk 
        ? (isMatch 
            ? `<span class="px-2 py-0.5 rounded text-[11px] font-semibold bg-emerald-500/10 text-emerald-400 border border-emerald-500/20">MATCH</span>` 
            : `<span class="px-2 py-0.5 rounded text-[11px] font-semibold bg-red-500/10 text-red-400 border border-red-500/20">MISMATCH</span>`)
        : `<span class="px-2 py-0.5 rounded text-[11px] font-semibold bg-amber-500/10 text-amber-400 border border-amber-500/20">${r.status}</span>`;

      return `
        <div class="p-3 rounded-lg border ${isOk && isMatch ? 'border-[var(--border)]' : 'border-amber-500/30'} bg-[var(--card)] flex flex-col space-y-1.5 shadow-xs transition hover:border-[var(--muted-foreground)]">
          <div class="flex items-center justify-between">
            <div class="flex items-center space-x-2">
              <span class="font-bold text-xs text-[var(--foreground)]">${r.name}</span>
              <span class="text-[11px] text-[var(--muted-foreground)] font-mono">(${r.tech})</span>
            </div>
            <div class="flex items-center space-x-2">
              ${r.micros !== undefined ? `<span class="text-[11px] font-mono text-[var(--muted-foreground)]">${r.micros} &mu;s</span>` : ''}
              ${statusBadge}
            </div>
          </div>
          <div class="font-mono text-xs text-[var(--foreground)] break-all select-all bg-[var(--background)] px-2 py-1.5 rounded border border-[var(--border)]">
            ${r.digest ? r.digest : `<span class="text-[var(--muted-foreground)] italic font-sans">${r.error || 'Runtime unavailable or failed'}</span>`}
          </div>
        </div>
      `;
    }).join('');
  }

  _renderBrowserOnly() {
    this._renderMatrix({
      browser: {
        name: 'Browser JS (WebCrypto)',
        tech: 'SubtleCrypto SHA-256',
        status: this.browserDigest ? 'OK' : 'ERROR',
        digest: this.browserDigest,
        micros: 0
      },
      cpp: { name: 'C++20 Native Engine', tech: 'GCC 16.2.0', status: 'WAITING' },
      rust: { name: 'Rust Conformance Binary', tech: 'Rust 1.85.0', status: 'WAITING' },
      go: { name: 'Go Conformance Binary', tech: 'Go 1.24.1', status: 'WAITING' },
      python: { name: 'Python Reference Core', tech: 'Python 3.14.7', status: 'WAITING' }
    });
  }

  _renderEmptyGrid() {
    this.consensusBanner.className = 'p-3.5 rounded-xl border border-[var(--border)] bg-[var(--background)] flex items-center justify-between';
    this.consensusIcon.className = 'w-7 h-7 rounded-lg bg-[var(--secondary)] flex items-center justify-center font-bold text-xs text-[var(--muted-foreground)]';
    this.consensusIcon.innerHTML = '--';
    this.consensusTitle.innerText = 'Awaiting Valid Envelope';
    this.consensusDesc.innerText = 'Enter or select a valid envelope JSON to execute conformance comparison';
    this.runtimeGrid.innerHTML = `
      <div class="p-6 text-center text-xs text-[var(--muted-foreground)] border border-dashed border-[var(--border)] rounded-lg">
        Select a preset from the left pane or enter an envelope JSON to run multi-runtime comparison.
      </div>
    `;
  }
}
