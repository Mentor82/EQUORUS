// EQUORUS Studio - LiNeP v0.2 Request Adapter

export const LINEP_TYPE_ID = 'linep.v02.request';

/**
 * Inspects a LiNeP v0.2 request envelope.
 *
 * @param {any} env
 * @returns {Object|null}
 */
export function inspectLinepEnvelope(env) {
  if (!env || env.type_id !== LINEP_TYPE_ID) {
    return null;
  }

  const p = env.payload || {};
  const stream = p.stream || {};
  const hasOptions = p.has_options === true;
  const opt = p.options || {};
  const extraOptions = Array.isArray(opt.extra_options) ? opt.extra_options : [];

  let sortedKeysValid = true;
  const keys = extraOptions.map(pair => Array.isArray(pair) ? pair[0] : '');
  for (let i = 1; i < keys.length; i++) {
    if (keys[i - 1] >= keys[i]) {
      sortedKeysValid = false;
      break;
    }
  }

  return {
    isLinep: true,
    protocolVersion: p.protocol_version || '0.2',
    profile: p.profile || 'unknown',
    modelId: p.model_id || 'unknown',
    promptPayload: p.payload || '',
    maxTokens: p.max_tokens,
    temperature: p.temperature,
    streamRequested: p.stream_requested === true,
    stream: {
      requestId: stream.request_id || '0',
      executionId: stream.execution_id || '0',
      outputId: stream.output_id || 0
    },
    hasOptions,
    options: {
      topP: opt.top_p,
      topK: opt.top_k,
      seed: opt.seed || '0',
      repeatPenalty: opt.repeat_penalty,
      presencePenalty: opt.presence_penalty,
      frequencyPenalty: opt.frequency_penalty,
      stopSequences: Array.isArray(opt.stop_sequences) ? opt.stop_sequences : []
    },
    extraOptions,
    sortedKeysValid
  };
}
