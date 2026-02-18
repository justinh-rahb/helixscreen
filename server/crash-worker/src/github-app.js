// SPDX-License-Identifier: GPL-3.0-or-later
//
// GitHub App authentication for Cloudflare Workers.
// Generates installation tokens using JWT + RS256 via Web Crypto API.

const GITHUB_API = "https://api.github.com";

// =============================================================================
// PEM → CryptoKey
// =============================================================================

/**
 * Import a PEM-encoded RSA private key for RS256 signing.
 * Handles both PKCS#1 (BEGIN RSA PRIVATE KEY) and PKCS#8 (BEGIN PRIVATE KEY).
 */
export async function importPrivateKey(pem) {
  const pemBody = pem
    .replace(/-----BEGIN RSA PRIVATE KEY-----/, "")
    .replace(/-----END RSA PRIVATE KEY-----/, "")
    .replace(/-----BEGIN PRIVATE KEY-----/, "")
    .replace(/-----END PRIVATE KEY-----/, "")
    .replace(/\s/g, "");

  const binaryDer = Uint8Array.from(atob(pemBody), (c) => c.charCodeAt(0));
  const isPkcs8 = pem.includes("BEGIN PRIVATE KEY");

  // GitHub App keys are PKCS#1 — need to wrap in PKCS#8 for Web Crypto
  const keyData = isPkcs8 ? binaryDer : wrapPkcs1InPkcs8(binaryDer);

  return crypto.subtle.importKey(
    "pkcs8",
    keyData,
    { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" },
    false,
    ["sign"]
  );
}

// =============================================================================
// ASN.1 helpers for PKCS#1 → PKCS#8 wrapping
// =============================================================================

function wrapPkcs1InPkcs8(pkcs1) {
  // PKCS#8 envelope: SEQUENCE { INTEGER 0, SEQUENCE { OID rsaEncryption, NULL }, OCTET STRING { pkcs1 } }
  const rsaOid = new Uint8Array([
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
    0x01, 0x05, 0x00,
  ]);
  const version = new Uint8Array([0x02, 0x01, 0x00]);
  const octetString = wrapAsn1(0x04, pkcs1);
  return wrapAsn1(0x30, concatBytes(version, rsaOid, octetString));
}

function wrapAsn1(tag, content) {
  const len = encodeAsn1Length(content.length);
  const result = new Uint8Array(1 + len.length + content.length);
  result[0] = tag;
  result.set(len, 1);
  result.set(content, 1 + len.length);
  return result;
}

function encodeAsn1Length(length) {
  if (length < 0x80) return new Uint8Array([length]);
  const bytes = [];
  let tmp = length;
  while (tmp > 0) {
    bytes.unshift(tmp & 0xff);
    tmp >>= 8;
  }
  return new Uint8Array([0x80 | bytes.length, ...bytes]);
}

function concatBytes(...arrays) {
  const total = arrays.reduce((sum, a) => sum + a.length, 0);
  const result = new Uint8Array(total);
  let offset = 0;
  for (const a of arrays) {
    result.set(a, offset);
    offset += a.length;
  }
  return result;
}

// =============================================================================
// JWT
// =============================================================================

function base64url(data) {
  const bytes = typeof data === "string" ? new TextEncoder().encode(data) : data;
  return btoa(String.fromCharCode(...bytes))
    .replace(/\+/g, "-")
    .replace(/\//g, "_")
    .replace(/=+$/, "");
}

async function createJWT(appId, privateKey) {
  const now = Math.floor(Date.now() / 1000);
  const header = { alg: "RS256", typ: "JWT" };
  const payload = {
    iat: now - 60,
    exp: now + 600,
    iss: appId,
  };

  const headerB64 = base64url(JSON.stringify(header));
  const payloadB64 = base64url(JSON.stringify(payload));
  const signingInput = `${headerB64}.${payloadB64}`;

  const signature = await crypto.subtle.sign(
    "RSASSA-PKCS1-v1_5",
    privateKey,
    new TextEncoder().encode(signingInput)
  );

  return `${signingInput}.${base64url(new Uint8Array(signature))}`;
}

// =============================================================================
// GitHub API
// =============================================================================

async function githubFetch(path, token, options = {}) {
  return fetch(`${GITHUB_API}${path}`, {
    ...options,
    headers: {
      Accept: "application/vnd.github+json",
      Authorization: `Bearer ${token}`,
      "X-GitHub-Api-Version": "2022-11-28",
      "User-Agent": "HelixScreen-Crash-Reporter",
      ...(options.headers || {}),
    },
  });
}

/**
 * Get a short-lived installation access token for the GitHub App.
 * @param {string} appId - GitHub App ID
 * @param {string} pemKey - PEM-encoded private key
 * @param {string} owner - Repo owner
 * @param {string} repo - Repo name
 * @returns {Promise<string>} Installation access token
 */
export async function getInstallationToken(appId, pemKey, owner, repo) {
  const key = await importPrivateKey(pemKey);
  const jwt = await createJWT(appId, key);

  // Get installation ID for the repo
  const installRes = await githubFetch(
    `/repos/${owner}/${repo}/installation`,
    jwt
  );
  if (!installRes.ok) {
    const body = await installRes.text();
    throw new Error(`Failed to get installation: ${installRes.status} ${body}`);
  }
  const { id } = await installRes.json();

  // Create installation access token
  const tokenRes = await githubFetch(
    `/app/installations/${id}/access_tokens`,
    jwt,
    { method: "POST" }
  );
  if (!tokenRes.ok) {
    const body = await tokenRes.text();
    throw new Error(`Failed to create token: ${tokenRes.status} ${body}`);
  }

  const { token } = await tokenRes.json();
  return token;
}

// =============================================================================
// Crash fingerprint (for dedup)
// =============================================================================

/**
 * Generate a short fingerprint for a crash to detect duplicates.
 * Based on: signal + version + first backtrace frame
 */
export function crashFingerprint(report) {
  const sig = report.signal_name || `SIG${report.signal}`;
  const ver = report.app_version || "unknown";
  const frame = report.backtrace?.[0] || "no-bt";
  return `${sig}/${ver}/${frame}`;
}

/**
 * Search for an existing open issue with the same crash fingerprint.
 * @returns {Promise<{number: number, html_url: string} | null>}
 */
export async function findExistingIssue(token, owner, repo, fingerprint) {
  const query = encodeURIComponent(
    `repo:${owner}/${repo} is:issue is:open label:crash "${fingerprint}" in:body`
  );
  const res = await githubFetch(`/search/issues?q=${query}&per_page=1`, token);
  if (!res.ok) return null;

  const data = await res.json();
  return data.total_count > 0 ? data.items[0] : null;
}

/**
 * Add a comment to an existing issue noting an additional occurrence.
 */
export async function addDuplicateComment(token, owner, repo, issueNumber, report, fingerprint) {
  const comment = [
    `## Additional occurrence`,
    "",
    `Another device reported this crash:`,
    `- **Timestamp:** ${report.timestamp || new Date().toISOString()}`,
    `- **Platform:** ${report.platform || "unknown"}`,
    `- **Uptime:** ${report.uptime_seconds != null ? report.uptime_seconds + "s" : "unknown"}`,
    report.ram_mb ? `- **RAM:** ${report.ram_mb} MB` : "",
    report.printer_model ? `- **Printer:** ${report.printer_model}` : "",
    "",
    `<sub>Fingerprint: \`${fingerprint}\`</sub>`,
  ]
    .filter(Boolean)
    .join("\n");

  await githubFetch(`/repos/${owner}/${repo}/issues/${issueNumber}/comments`, token, {
    method: "POST",
    body: JSON.stringify({ body: comment }),
  });
}
