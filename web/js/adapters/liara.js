// EQUORUS Studio - L.I.A.R.A. Heartbeat Adapter

export const LIARA_TYPE_ID = 'liara.heartbeat.snapshot';

/**
 * Inspects a LIARA heartbeat snapshot envelope.
 *
 * @param {any} env
 * @returns {Object|null}
 */
export function inspectLiaraEnvelope(env) {
  if (!env || env.type_id !== LIARA_TYPE_ID) {
    return null;
  }

  const p = env.payload || {};
  const observations = Array.isArray(p.observations) ? p.observations : [];

  let attributesEmptyInvariant = true;
  for (const obs of observations) {
    if (obs && obs.attributes && Object.keys(obs.attributes).length > 0) {
      attributesEmptyInvariant = false;
      break;
    }
  }

  return {
    isLiara: true,
    instanceId: p.instance_id || 'unknown',
    instanceType: p.instance_type || 'heartbeat',
    nodeId: p.node_id || 'unknown',
    sequence: p.sequence || '0',
    state: p.state || 'unknown',
    confidence: typeof p.confidence === 'number' ? p.confidence : null,
    observedAt: p.observed_at || 'unknown',
    observationCount: observations.length,
    observations: observations.map((o, idx) => ({
      index: idx,
      resource: o.resource || 'unknown',
      metric: o.metric || 'unknown',
      value: o.value,
      unit: o.unit || '',
      confidence: o.confidence,
      sourceId: o.source_id || 'unknown',
      attributesValid: o.attributes && Object.keys(o.attributes).length === 0
    })),
    signalsCount: Array.isArray(p.signals) ? p.signals.length : 0,
    attributesEmptyInvariant
  };
}
