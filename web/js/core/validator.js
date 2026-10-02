// EQUORUS Studio - Core Specification Validator (v0.1)

/**
 * Validation result object.
 * @typedef {Object} ValidationResult
 * @property {boolean} valid - Whether the input complies with EQUORUS v0.1 rules.
 * @property {string|null} typeId - Identified envelope type_id.
 * @property {string|null} schemaVersion - Identified schema_version.
 * @property {string[]} errors - Array of error messages.
 * @property {string[]} warnings - Array of warning messages.
 * @property {any} parsedObject - The successfully parsed object, or null.
 */

export class EquorusValidator {
  /**
   * Validates a raw JSON string against the EQUORUS v0.1 specification.
   * Checks for duplicate keys, lone surrogates, invalid numbers (-0.0, NaN),
   * and envelope structure invariants.
   *
   * @param {string} rawJson
   * @returns {ValidationResult}
   */
  static validate(rawJson) {
    const errors = [];
    const warnings = [];

    if (!rawJson || typeof rawJson !== 'string' || rawJson.trim().length === 0) {
      return { valid: false, typeId: null, schemaVersion: null, errors: ['Input is empty'], warnings, parsedObject: null };
    }

    // 1. Lexical checks: duplicate keys and lone surrogates
    const dupCheck = this._detectDuplicateKeys(rawJson);
    if (dupCheck.hasDuplicates) {
      errors.push(`DUPLICATE_KEY: Duplicate key "${dupCheck.duplicateKey}" detected in JSON object`);
    }

    const surrogateCheck = this._detectLoneSurrogates(rawJson);
    if (surrogateCheck.hasLoneSurrogate) {
      errors.push(`UNICODE: Lone surrogate detected at offset ${surrogateCheck.offset} ("${surrogateCheck.sequence}")`);
    }

    // 2. Lexical number check: negative zero (-0, -0.0)
    if (/(^|[,\:\[\{])\s*-\s*0(\.0+)?([eE][+-]?\d+)?\s*([,\}\]:]|$)/.test(rawJson)) {
      errors.push('NUMBER: Negative zero (-0.0) is strictly forbidden in EQUORUS v0.1');
    }

    // 3. Parse JSON
    let parsed = null;
    try {
      parsed = JSON.parse(rawJson);
    } catch (e) {
      errors.push(`MALFORMED_JSON: ${e.message}`);
      return { valid: false, typeId: null, schemaVersion: null, errors, warnings, parsedObject: null };
    }

    // 4. Value structure & semantic checks
    if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) {
      errors.push('ENVELOPE: Top-level EQUORUS entity must be a JSON object');
      return { valid: false, typeId: null, schemaVersion: null, errors, warnings, parsedObject: parsed };
    }

    // Check envelope envelope fields
    const requiredTopFields = ['type_id', 'schema_version', 'provenance', 'payload'];
    for (const f of requiredTopFields) {
      if (!(f in parsed)) {
        errors.push(`ENVELOPE_SCHEMA: Missing required top-level field "${f}"`);
      }
    }

    const typeId = typeof parsed.type_id === 'string' ? parsed.type_id : null;
    const schemaVersion = typeof parsed.schema_version === 'string' ? parsed.schema_version : null;

    if (typeId === null && 'type_id' in parsed) {
      errors.push('ENVELOPE: "type_id" must be a non-empty string');
    }
    if (schemaVersion === null && 'schema_version' in parsed) {
      errors.push('ENVELOPE: "schema_version" must be a string');
    }

    // Provenance checks
    if ('provenance' in parsed) {
      if (typeof parsed.provenance !== 'object' || parsed.provenance === null || Array.isArray(parsed.provenance)) {
        errors.push('PROVENANCE: "provenance" must be a non-null object');
      } else {
        if (!('kind' in parsed.provenance)) {
          errors.push('PROVENANCE: Missing required field "kind" in provenance');
        } else {
          const validKinds = ['SOURCE_LITERAL', 'DERIVED_PROMPT', 'DERIVED_CONTEXT', 'TOOL_EVIDENCE', 'USER_EDIT'];
          if (!validKinds.includes(parsed.provenance.kind)) {
            warnings.push(`PROVENANCE: Non-standard provenance kind "${parsed.provenance.kind}"`);
          }
        }

        if ('source_id' in parsed.provenance && parsed.provenance.source_id !== null && typeof parsed.provenance.source_id !== 'string') {
          errors.push('PROVENANCE: "source_id" must be a string or null');
        }

        if ('timestamp_ms' in parsed.provenance) {
          if (!this.isValidUint64String(parsed.provenance.timestamp_ms)) {
            errors.push('PROVENANCE: "timestamp_ms" must be a valid unsigned 64-bit integer decimal string');
          }
        }
      }
    }

    // Payload checks
    if ('payload' in parsed) {
      if (typeof parsed.payload !== 'object' || parsed.payload === null) {
        errors.push('PAYLOAD: "payload" must be a JSON object');
      }
    }

    // Domain-specific invariant checks
    if (typeId === 'liara.heartbeat.snapshot') {
      this._validateLiaraHeartbeat(parsed, errors, warnings);
    } else if (typeId === 'vinox.provenance.snapshot') {
      this._validateVinoxProvenance(parsed, errors, warnings);
    } else if (typeId === 'linep.v02.request') {
      this._validateLinepRequest(parsed, errors, warnings);
    }

    // Deep check for integer precision limits (-2^53 + 1 to 2^53 - 1)
    this._checkNumberPrecisions(parsed, '', errors);

    return {
      valid: errors.length === 0,
      typeId,
      schemaVersion,
      errors,
      warnings,
      parsedObject: parsed
    };
  }

  /**
   * Verifies if a string is a valid unsigned 64-bit decimal string without sign or leading zeros.
   * Max uint64: 18446744073709551615
   * @param {any} val
   * @returns {boolean}
   */
  static isValidUint64String(val) {
    if (typeof val !== 'string') return false;
    if (!/^(0|[1-9]\d*)$/.test(val)) return false;
    try {
      const b = BigInt(val);
      const MAX_U64 = 18446744073709551615n;
      return b >= 0n && b <= MAX_U64;
    } catch {
      return false;
    }
  }

  static _detectDuplicateKeys(jsonText) {
    let duplicateKey = null;
    let hasDuplicates = false;
    const stack = [new Set()];

    // A lightweight tokenizer to catch object keys
    const re = /"((?:\\.|[^"\\])*)"\s*:\s*|([\{\}\[\]])/g;
    let match;
    while ((match = re.exec(jsonText)) !== null) {
      if (match[1] !== undefined) {
        const key = match[1];
        const currentSet = stack[stack.length - 1];
        if (currentSet) {
          if (currentSet.has(key)) {
            duplicateKey = key;
            hasDuplicates = true;
            break;
          }
          currentSet.add(key);
        }
      } else if (match[2] === '{') {
        stack.push(new Set());
      } else if (match[2] === '}') {
        stack.pop();
      } else if (match[2] === '[') {
        stack.push(null); // array scope
      } else if (match[2] === ']') {
        stack.pop();
      }
    }
    return { hasDuplicates, duplicateKey };
  }

  static _detectLoneSurrogates(text) {
    // Lone escaped surrogate pattern like \ud800 not followed by \udc00..\udfff
    const escapedSurrogateRe = /\\u([dD][89abAB][0-9a-fA-F]{2})(?!\\u[dD][c-fC-F][0-9a-fA-F]{2})/g;
    let match = escapedSurrogateRe.exec(text);
    if (match) {
      return { hasLoneSurrogate: true, sequence: match[0], offset: match.index };
    }
    const trailingSurrogateRe = /(?<!\\u[dD][89abAB][0-9a-fA-F]{2})\\u([dD][c-fC-F][0-9a-fA-F]{2})/g;
    match = trailingSurrogateRe.exec(text);
    if (match) {
      return { hasLoneSurrogate: true, sequence: match[0], offset: match.index };
    }
    return { hasLoneSurrogate: false };
  }

  static _checkNumberPrecisions(val, path, errors) {
    if (typeof val === 'number') {
      if (!Number.isFinite(val)) {
        errors.push(`NUMBER: Non-finite number at "${path}" (${val})`);
      } else if (Number.isInteger(val)) {
        if (val > Number.MAX_SAFE_INTEGER || val < Number.MIN_SAFE_INTEGER) {
          errors.push(`NUMBER: Integer at "${path}" exceeds safe binary64 integer range [-(2^53-1), 2^53-1]`);
        }
      }
    } else if (Array.isArray(val)) {
      for (let i = 0; i < val.length; i++) {
        this._checkNumberPrecisions(val[i], `${path}[${i}]`, errors);
      }
    } else if (val !== null && typeof val === 'object') {
      for (const [k, v] of Object.entries(val)) {
        this._checkNumberPrecisions(v, path ? `${path}.${k}` : k, errors);
      }
    }
  }

  static _validateLiaraHeartbeat(env, errors, warnings) {
    const payload = env.payload || {};
    if (!('sequence' in payload) || !this.isValidUint64String(payload.sequence)) {
      errors.push('LIARA_HEARTBEAT: "sequence" must be a valid unsigned 64-bit decimal string');
    }
    if (Array.isArray(payload.observations)) {
      for (let i = 0; i < payload.observations.length; i++) {
        const obs = payload.observations[i];
        if (obs && 'attributes' in obs) {
          if (typeof obs.attributes !== 'object' || obs.attributes === null || Object.keys(obs.attributes).length > 0) {
            errors.push(`LIARA_HEARTBEAT: Observation[${i}] has non-empty "attributes". EQUORUS pilot v0.1 requires strictly empty {}`);
          }
        }
      }
    }
  }

  static _validateVinoxProvenance(env, errors, warnings) {
    // VINOX schema validations
    const prov = env.provenance || {};
    if (!prov.kind) {
      errors.push('VINOX_PROVENANCE: Missing "kind" in provenance');
    }
  }

  static _validateLinepRequest(env, errors, warnings) {
    const payload = env.payload || {};
    if (payload.has_options === true) {
      if (!payload.options || typeof payload.options !== 'object') {
        errors.push('LINEP_REQUEST: "options" must be an object when "has_options" is true');
      } else if (Array.isArray(payload.options.extra_options)) {
        // Check sorting and duplicates of extra_options
        const keys = [];
        for (const pair of payload.options.extra_options) {
          if (!Array.isArray(pair) || pair.length !== 2 || typeof pair[0] !== 'string') {
            errors.push('LINEP_REQUEST: extra_options must be an array of [key, value] pairs');
            return;
          }
          keys.push(pair[0]);
        }
        for (let i = 1; i < keys.length; i++) {
          if (keys[i - 1] === keys[i]) {
            errors.push(`LINEP_REQUEST: Duplicate key "${keys[i]}" in extra_options`);
          } else if (keys[i - 1] > keys[i]) {
            errors.push(`LINEP_REQUEST: extra_options keys must be strictly sorted lexicographically by UTF-8 bytes ("${keys[i-1]}" before "${keys[i]}")`);
          }
        }
      }
    }
  }
}
