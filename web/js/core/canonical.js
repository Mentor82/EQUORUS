// EQUORUS Studio - Canonical Byte Serializer (equorus-value-v1)

const encoder = new TextEncoder();

/**
 * Compare two strings lexicographically by their unsigned UTF-8 byte sequences.
 * Embedded NUL bytes are compared normally; shorter equal prefix sorts first.
 *
 * @param {string} a
 * @param {string} b
 * @returns {number}
 */
export function compareUtf8Bytes(a, b) {
  const bytesA = encoder.encode(a);
  const bytesB = encoder.encode(b);
  const minLen = Math.min(bytesA.length, bytesB.length);
  for (let i = 0; i < minLen; i++) {
    if (bytesA[i] !== bytesB[i]) {
      return bytesA[i] - bytesB[i];
    }
  }
  return bytesA.length - bytesB.length;
}

/**
 * Serializes any EQUORUS value tree into equorus-value-v1 canonical bytes.
 *
 * @param {any} value
 * @returns {Uint8Array}
 */
export function serializeCanonicalBytes(value) {
  const chunks = [];

  function appendBytes(u8) {
    chunks.push(u8);
  }

  function appendByte(b) {
    chunks.push(new Uint8Array([b]));
  }

  function appendU64(n) {
    const buf = new ArrayBuffer(8);
    const view = new DataView(buf);
    view.setBigUint64(0, BigInt(n), false); // Big-endian
    chunks.push(new Uint8Array(buf));
  }

  function appendString(str) {
    appendByte(0x73); // 's'
    const utf8 = encoder.encode(str);
    appendU64(utf8.length);
    appendBytes(utf8);
  }

  function walk(val) {
    if (val === null) {
      appendByte(0x6e); // 'n'
    } else if (typeof val === 'boolean') {
      appendByte(val ? 0x74 : 0x66); // 't' or 'f'
    } else if (typeof val === 'number') {
      appendByte(0x64); // 'd'
      const buf = new ArrayBuffer(8);
      const view = new DataView(buf);
      view.setFloat64(0, val, false); // Big-endian IEEE-754 binary64
      chunks.push(new Uint8Array(buf));
    } else if (typeof val === 'string') {
      appendString(val);
    } else if (Array.isArray(val)) {
      appendByte(0x61); // 'a'
      appendU64(val.length);
      for (const item of val) {
        walk(item);
      }
    } else if (typeof val === 'object') {
      appendByte(0x6f); // 'o'
      const keys = Object.keys(val);
      appendU64(keys.length);
      keys.sort(compareUtf8Bytes);
      for (const k of keys) {
        appendString(k);
        walk(val[k]);
      }
    } else {
      throw new Error(`Unsupported value type: ${typeof val}`);
    }
  }

  walk(value);

  // Combine chunks into a single Uint8Array
  let totalLength = 0;
  for (const c of chunks) {
    totalLength += c.length;
  }
  const result = new Uint8Array(totalLength);
  let offset = 0;
  for (const c of chunks) {
    result.set(c, offset);
    offset += c.length;
  }
  return result;
}

/**
 * Convert a Uint8Array into a hex string.
 * @param {Uint8Array} bytes
 * @returns {string}
 */
export function bytesToHex(bytes) {
  let hex = '';
  for (let i = 0; i < bytes.length; i++) {
    hex += bytes[i].toString(16).padStart(2, '0');
  }
  return hex;
}

/**
 * Format bytes into a readable multi-line hex dump with ASCII sidebar.
 * @param {Uint8Array} bytes
 * @param {number} bytesPerLine
 * @returns {string}
 */
export function formatHexDump(bytes, bytesPerLine = 16) {
  let out = '';
  for (let i = 0; i < bytes.length; i += bytesPerLine) {
    const chunk = bytes.slice(i, i + bytesPerLine);
    const offset = i.toString(16).padStart(6, '0');
    let hexPart = '';
    let asciiPart = '';

    for (let j = 0; j < bytesPerLine; j++) {
      if (j < chunk.length) {
        hexPart += chunk[j].toString(16).padStart(2, '0') + ' ';
        const b = chunk[j];
        asciiPart += (b >= 32 && b <= 126) ? String.fromCharCode(b) : '.';
      } else {
        hexPart += '   ';
      }
      if (j === 7) hexPart += ' ';
    }

    out += `${offset}  ${hexPart} |${asciiPart}|\n`;
  }
  return out;
}
