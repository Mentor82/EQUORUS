// EQUORUS Studio - Consumer Adapter Visualizer Component

import { inspectVinoxEnvelope } from '../adapters/vinox.js';
import { inspectLiaraEnvelope } from '../adapters/liara.js';
import { inspectLinepEnvelope } from '../adapters/linep.js';

export class AdapterView {
  constructor({ container }) {
    this.container = container;
    this.render();
  }

  render() {
    this.container.innerHTML = `
      <div id="adapter-content" class="flex flex-col h-full space-y-4">
        <div class="p-6 text-center text-[var(--muted-foreground)] text-xs">
          Select a consumer preset or paste a known envelope (VINOX, LIARA, LiNeP) to visualize domain adapters.
        </div>
      </div>
    `;
    this.contentEl = this.container.querySelector('#adapter-content');
  }

  update(env) {
    if (!env || typeof env !== 'object') {
      this.contentEl.innerHTML = `
        <div class="p-8 text-center text-[var(--muted-foreground)] text-xs">
          No active envelope loaded.
        </div>
      `;
      return;
    }

    const vinox = inspectVinoxEnvelope(env);
    if (vinox) {
      this._renderVinox(vinox);
      return;
    }

    const liara = inspectLiaraEnvelope(env);
    if (liara) {
      this._renderLiara(liara);
      return;
    }

    const linep = inspectLinepEnvelope(env);
    if (linep) {
      this._renderLinep(linep);
      return;
    }

    // Generic envelope fallback
    this.contentEl.innerHTML = `
      <div class="p-4 rounded-xl border border-[var(--border)] bg-[var(--background)] space-y-3">
        <div class="flex items-center space-x-2">
          <span class="text-xs uppercase font-semibold text-purple-400 font-mono">Generic Envelope</span>
        </div>
        <div class="grid grid-cols-2 gap-2 text-xs">
          <div><span class="text-[var(--muted-foreground)]">Type ID:</span> <span class="font-mono">${env.type_id || 'unknown'}</span></div>
          <div><span class="text-[var(--muted-foreground)]">Schema Version:</span> <span class="font-mono">${env.schema_version || 'unknown'}</span></div>
        </div>
      </div>
    `;
  }

  _renderVinox(v) {
    this.contentEl.innerHTML = `
      <div class="space-y-4">
        <div class="p-4 rounded-xl border border-purple-500/30 bg-purple-500/10 flex items-center justify-between">
          <div class="flex items-center space-x-3">
            <span class="text-xs font-bold uppercase tracking-wider px-2 py-0.5 rounded bg-purple-500/20 text-purple-300">VINOX PROVENANCE</span>
            <span class="text-xs text-[var(--muted-foreground)]">Snapshot Adapter</span>
          </div>
          <span class="text-xs font-mono font-semibold px-2 py-0.5 rounded bg-purple-500/30 text-purple-200">
            ${v.kind} (id=${v.kindNumeric})
          </span>
        </div>

        <div class="grid grid-cols-2 gap-3 text-xs">
          <div class="p-3 rounded-lg border border-[var(--border)] bg-[var(--background)]">
            <div class="text-[var(--muted-foreground)]">Source ID</div>
            <div class="font-mono text-sm font-semibold mt-1 text-[var(--foreground)] truncate">${v.sourceId}</div>
          </div>
          <div class="p-3 rounded-lg border border-[var(--border)] bg-[var(--background)]">
            <div class="text-[var(--muted-foreground)]">Timestamp (ms)</div>
            <div class="font-mono text-sm font-semibold mt-1 text-[var(--foreground)]">${v.timestampMs}</div>
            <div class="text-[11px] text-[var(--muted-foreground)] mt-0.5">${v.formattedDate}</div>
          </div>
        </div>
      </div>
    `;
  }

  _renderLiara(l) {
    const invariantBadge = l.attributesEmptyInvariant
      ? `<span class="text-xs px-2 py-0.5 rounded bg-emerald-500/20 text-emerald-300 font-mono">Attributes {} Invariant PASS</span>`
      : `<span class="text-xs px-2 py-0.5 rounded bg-rose-500/20 text-rose-300 font-mono">Attributes {} Invariant VIOLATION</span>`;

    const obsRows = l.observations.map(o => `
      <tr class="border-b border-[var(--border)]/40 hover:bg-[var(--card)]/50">
        <td class="p-2 font-mono">${o.resource}</td>
        <td class="p-2 font-mono">${o.metric}</td>
        <td class="p-2 font-mono text-cyan-300">${o.value} ${o.unit}</td>
        <td class="p-2 font-mono text-[var(--muted-foreground)]">${o.sourceId}</td>
        <td class="p-2 text-center">
          ${o.attributesValid ? '<span class="text-emerald-400">✓</span>' : '<span class="text-rose-400">✗</span>'}
        </td>
      </tr>
    `).join('');

    this.contentEl.innerHTML = `
      <div class="space-y-4">
        <div class="p-4 rounded-xl border border-blue-500/30 bg-blue-500/10 flex items-center justify-between">
          <div class="flex items-center space-x-3">
            <span class="text-xs font-bold uppercase tracking-wider px-2 py-0.5 rounded bg-blue-500/20 text-blue-300">L.I.A.R.A. RUNTIME</span>
            <span class="text-xs text-[var(--muted-foreground)]">Heartbeat Adapter</span>
          </div>
          <div>${invariantBadge}</div>
        </div>

        <div class="grid grid-cols-3 gap-3 text-xs">
          <div class="p-3 rounded-lg border border-[var(--border)] bg-[var(--background)]">
            <div class="text-[var(--muted-foreground)]">Sequence (uint64)</div>
            <div class="font-mono text-sm font-semibold mt-1 text-[var(--foreground)]">${l.sequence}</div>
          </div>
          <div class="p-3 rounded-lg border border-[var(--border)] bg-[var(--background)]">
            <div class="text-[var(--muted-foreground)]">Node / Instance</div>
            <div class="font-mono text-sm font-semibold mt-1 text-[var(--foreground)] truncate">${l.nodeId}</div>
          </div>
          <div class="p-3 rounded-lg border border-[var(--border)] bg-[var(--background)]">
            <div class="text-[var(--muted-foreground)]">Health State</div>
            <div class="font-mono text-sm font-semibold mt-1 uppercase text-emerald-400">${l.state}</div>
          </div>
        </div>

        <div class="space-y-1.5">
          <div class="text-xs font-semibold text-[var(--foreground)]">Observations (${l.observationCount}):</div>
          <div class="border border-[var(--border)] rounded-lg overflow-x-auto max-h-48">
            <table class="w-full text-xs text-left">
              <thead class="bg-[var(--card)] text-[var(--muted-foreground)] border-b border-[var(--border)]">
                <tr>
                  <th class="p-2">Resource</th>
                  <th class="p-2">Metric</th>
                  <th class="p-2">Value</th>
                  <th class="p-2">Source</th>
                  <th class="p-2 text-center">Attributes {}</th>
                </tr>
              </thead>
              <tbody>${obsRows}</tbody>
            </table>
          </div>
        </div>
      </div>
    `;
  }

  _renderLinep(lp) {
    const optRows = lp.extraOptions.map(pair => `
      <tr class="border-b border-[var(--border)]/40 hover:bg-[var(--card)]/50">
        <td class="p-2 font-mono text-amber-300">${pair[0]}</td>
        <td class="p-2 font-mono">${pair[1]}</td>
      </tr>
    `).join('');

    this.contentEl.innerHTML = `
      <div class="space-y-4">
        <div class="p-4 rounded-xl border border-emerald-500/30 bg-emerald-500/10 flex items-center justify-between">
          <div class="flex items-center space-x-3">
            <span class="text-xs font-bold uppercase tracking-wider px-2 py-0.5 rounded bg-emerald-500/20 text-emerald-300">LiNeP v0.2</span>
            <span class="text-xs text-[var(--muted-foreground)]">Request Adapter</span>
          </div>
          <span class="text-xs font-mono font-semibold px-2 py-0.5 rounded bg-emerald-500/30 text-emerald-200">
            Profile: ${lp.profile}
          </span>
        </div>

        <div class="grid grid-cols-3 gap-3 text-xs">
          <div class="p-3 rounded-lg border border-[var(--border)] bg-[var(--background)]">
            <div class="text-[var(--muted-foreground)]">Model ID</div>
            <div class="font-mono text-sm font-semibold mt-1 text-[var(--foreground)] truncate">${lp.modelId}</div>
          </div>
          <div class="p-3 rounded-lg border border-[var(--border)] bg-[var(--background)]">
            <div class="text-[var(--muted-foreground)]">Stream / Request ID</div>
            <div class="font-mono text-sm font-semibold mt-1 text-[var(--foreground)] truncate">${lp.stream.requestId}</div>
          </div>
          <div class="p-3 rounded-lg border border-[var(--border)] bg-[var(--background)]">
            <div class="text-[var(--muted-foreground)]">Temperature / Max Tokens</div>
            <div class="font-mono text-sm font-semibold mt-1 text-cyan-300">${lp.temperature ?? 'N/A'} / ${lp.maxTokens ?? 'N/A'}</div>
          </div>
        </div>

        ${lp.hasOptions ? `
          <div class="space-y-1.5">
            <div class="flex items-center justify-between text-xs">
              <span class="font-semibold text-[var(--foreground)]">Canonical Extra Options:</span>
              <span class="font-mono ${lp.sortedKeysValid ? 'text-emerald-400' : 'text-rose-400'}">
                ${lp.sortedKeysValid ? '✓ UTF-8 Lexicographically Sorted' : '✗ Unsorted or Duplicate Keys'}
              </span>
            </div>
            <div class="border border-[var(--border)] rounded-lg overflow-x-auto max-h-36">
              <table class="w-full text-xs text-left">
                <thead class="bg-[var(--card)] text-[var(--muted-foreground)] border-b border-[var(--border)]">
                  <tr>
                    <th class="p-2">Option Key</th>
                    <th class="p-2">Value</th>
                  </tr>
                </thead>
                <tbody>${optRows || '<tr><td colspan="2" class="p-2 text-[var(--muted-foreground)]">No extra options</td></tr>'}</tbody>
              </table>
            </div>
          </div>
        ` : ''}
      </div>
    `;
  }
}
