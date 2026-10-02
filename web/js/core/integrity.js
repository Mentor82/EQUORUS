// EQUORUS Studio - Detached SHA-256 Integrity Engine (v0.1)

import { serializeCanonicalBytes, bytesToHex } from './canonical.js';

export const PROFILE = 'equorus-value-v1';
export const ALGORITHM = 'sha-256';

const asciiEncoder = new TextEncoder();
// Preimage prefix: ASCII("EQUORUS-INTEGRITY\0v1\0equorus-value-v1\0sha-256\0")
const PREIMAGE_HEADER = new Uint8Array([
  ...asciiEncoder.encode('EQUORUS-INTEGRITY'), 0x00,
  ...asciiEncoder.encode('v1'), 0x00,
  ...asciiEncoder.encode('equorus-value-v1'), 0x00,
  ...asciiEncoder.encode('sha-256'), 0x00
]);

/**
 * Constructs the domain-separated preimage buffer for a canonical byte array.
 * @param {Uint8Array} canonicalBytes
 * @returns {Uint8Array}
 */
export function buildPreimage(canonicalBytes) {
  const preimage = new Uint8Array(PREIMAGE_HEADER.length + canonicalBytes.length);
  preimage.set(PREIMAGE_HEADER, 0);
  preimage.set(canonicalBytes, PREIMAGE_HEADER.length);
  return preimage;
}

/**
 * Computes detached SHA-256 integrity record for any validated EQUORUS envelope object.
 *
 * @param {any} envelopeObj
 * @returns {Promise<{ record: { canonical_profile: string, algorithm: string, digest: string }, canonicalBytes: Uint8Array, preimage: Uint8Array, hexDigest: string }>}
 */
export async function computeEnvelopeIntegrity(envelopeObj) {
  const canonicalBytes = serializeCanonicalBytes(envelopeObj);
  const preimage = buildPreimage(canonicalBytes);

  let hashBuffer;
  if (typeof crypto !== 'undefined' && crypto.subtle && crypto.subtle.digest) {
    hashBuffer = await crypto.subtle.digest('SHA-256', preimage);
  } else {
    throw new Error('Web Crypto API (crypto.subtle.digest) is required for SHA-256 computation');
  }

  const hexDigest = bytesToHex(new Uint8Array(hashBuffer));
  const record = {
    canonical_profile: PROFILE,
    algorithm: ALGORITHM,
    digest: hexDigest
  };

  return {
    record,
    canonicalBytes,
    preimage,
    hexDigest
  };
}

/**
 * Verifies an envelope against a detached integrity record.
 *
 * @param {any} envelopeObj
 * @param {{ canonical_profile: string, algorithm: string, digest: string }} record
 * @returns {Promise<{ matches: boolean, actualDigest: string, expectedDigest: string }>}
 */
export async function verifyEnvelopeIntegrity(envelopeObj, record) {
  if (!record || record.canonical_profile !== PROFILE || record.algorithm !== ALGORITHM) {
    return { matches: false, actualDigest: '', expectedDigest: record ? record.digest : '' };
  }
  const computed = await computeEnvelopeIntegrity(envelopeObj);
  const matches = (computed.hexDigest.toLowerCase() === (record.digest || '').toLowerCase());
  return {
    matches,
    actualDigest: computed.hexDigest,
    expectedDigest: record.digest
  };
}
