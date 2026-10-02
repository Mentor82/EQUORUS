// EQUORUS Studio - VINOX Provenance Adapter

export const VINOX_TYPE_ID = 'vinox.provenance.snapshot';

export const VINOX_KINDS = {
  SOURCE_LITERAL: { id: 0, label: 'Source Literal', color: 'blue' },
  DERIVED_PROMPT: { id: 1, label: 'Derived Prompt', color: 'purple' },
  TOOL_EVIDENCE: { id: 2, label: 'Tool Evidence', color: 'emerald' },
  USER_EDIT: { id: 3, label: 'User Edit', color: 'amber' },
  DERIVED_CONTEXT: { id: 4, label: 'Derived Context', color: 'indigo' }
};

/**
 * Inspects a VINOX envelope and returns structured presentation details.
 *
 * @param {any} env
 * @returns {Object|null}
 */
export function inspectVinoxEnvelope(env) {
  if (!env || env.type_id !== VINOX_TYPE_ID) {
    return null;
  }

  const prov = env.provenance || {};
  const kindStr = prov.kind || 'UNKNOWN';
  const kindInfo = VINOX_KINDS[kindStr] || { id: -1, label: kindStr, color: 'gray' };

  let formattedDate = 'N/A';
  if (prov.timestamp_ms) {
    try {
      const ms = Number(BigInt(prov.timestamp_ms));
      if (ms > 0 && ms < 253402300799000) { // realistic date
        formattedDate = new Date(ms).toISOString();
      } else {
        formattedDate = `${prov.timestamp_ms} ms (Numeric)`;
      }
    } catch {
      formattedDate = prov.timestamp_ms;
    }
  }

  return {
    isVinox: true,
    kind: kindStr,
    kindNumeric: kindInfo.id,
    kindLabel: kindInfo.label,
    kindColor: kindInfo.color,
    sourceId: prov.source_id === null ? '(null / anonymous)' : (prov.source_id || '(omitted)'),
    timestampMs: prov.timestamp_ms || '0',
    formattedDate,
    payloadSize: Object.keys(env.payload || {}).length
  };
}
