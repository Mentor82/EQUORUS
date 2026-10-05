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
// Pure JS SHA-256 fallback for non-secure contexts (e.g. plain HTTP over LAN IP where crypto.subtle is undefined)
const K = new Uint32Array([
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
]);

function rotr(x, n) {
  return (x >>> n) | (x << (32 - n));
}

export function sha256Sync(bytes) {
  const bitLen = bytes.length * 8;
  const withPadLen = ((bytes.length + 9 + 63) >> 6) << 6;
  const padded = new Uint8Array(withPadLen);
  padded.set(bytes);
  padded[bytes.length] = 0x80;
  const view = new DataView(padded.buffer);
  view.setBigUint64(withPadLen - 8, BigInt(bitLen), false);

  let h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
  let h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;

  const w = new Uint32Array(64);

  for (let offset = 0; offset < withPadLen; offset += 64) {
    for (let i = 0; i < 16; i++) {
      w[i] = view.getUint32(offset + (i * 4), false);
    }
    for (let i = 16; i < 64; i++) {
      const s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >>> 3);
      const s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >>> 10);
      w[i] = (w[i - 16] + s0 + w[i - 7] + s1) | 0;
    }

    let a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;

    for (let i = 0; i < 64; i++) {
      const S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const ch = (e & f) ^ ((~e) & g);
      const temp1 = (h + S1 + ch + K[i] + w[i]) | 0;
      const S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const maj = (a & b) ^ (a & c) ^ (b & c);
      const temp2 = (S0 + maj) | 0;

      h = g;
      g = f;
      f = e;
      e = (d + temp1) | 0;
      d = c;
      c = b;
      b = a;
      a = (temp1 + temp2) | 0;
    }

    h0 = (h0 + a) | 0;
    h1 = (h1 + b) | 0;
    h2 = (h2 + c) | 0;
    h3 = (h3 + d) | 0;
    h4 = (h4 + e) | 0;
    h5 = (h5 + f) | 0;
    h6 = (h6 + g) | 0;
    h7 = (h7 + h) | 0;
  }

  const out = new Uint8Array(32);
  const outView = new DataView(out.buffer);
  outView.setUint32(0, h0, false);
  outView.setUint32(4, h1, false);
  outView.setUint32(8, h2, false);
  outView.setUint32(12, h3, false);
  outView.setUint32(16, h4, false);
  outView.setUint32(20, h5, false);
  outView.setUint32(24, h6, false);
  outView.setUint32(28, h7, false);
  return out;
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

  let hexDigest;
  if (typeof crypto !== 'undefined' && crypto.subtle && crypto.subtle.digest) {
    try {
      const hashBuffer = await crypto.subtle.digest('SHA-256', preimage);
      hexDigest = bytesToHex(new Uint8Array(hashBuffer));
    } catch {
      // Fallback if subtle.digest errors
      hexDigest = bytesToHex(sha256Sync(preimage));
    }
  } else {
    // Robust pure JS fallback when WebCrypto is unavailable (insecure context over LAN HTTP)
    hexDigest = bytesToHex(sha256Sync(preimage));
  }

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
