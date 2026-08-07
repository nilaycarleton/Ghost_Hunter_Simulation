import { createPublicKey, verify } from "node:crypto";

const jwksCache = new Map();

export function clerkRuntimeConfig() {
  const publishableKey = process.env.CLERK_PUBLISHABLE_KEY || process.env.NEXT_PUBLIC_CLERK_PUBLISHABLE_KEY || "";
  const frontendApi = normalizeClerkFrontendApi(process.env.CLERK_FRONTEND_API || decodeClerkFrontendApi(publishableKey));
  const issuer = process.env.CLERK_ISSUER || (frontendApi ? `https://${frontendApi}` : "");
  const jwksUrl = process.env.CLERK_JWKS_URL || (issuer ? `${issuer}/.well-known/jwks.json` : "");
  return {
    enabled: Boolean(publishableKey.startsWith("pk_") && frontendApi && issuer && jwksUrl),
    publishableKey,
    frontendApi,
    issuer,
    jwksUrl,
    authorizedParties: (process.env.CLERK_AUTHORIZED_PARTIES || "")
      .split(",").map(origin => origin.trim()).filter(Boolean),
  };
}

export async function optionalAuth(req) {
  const token = bearerToken(req) || cookieToken(req);
  if (!token) return null;
  try {
    return await verifyClerkToken(token, clerkRuntimeConfig());
  } catch (cause) {
    const error = new Error("Your sign-in session could not be verified.");
    error.statusCode = 401;
    error.cause = cause;
    throw error;
  }
}

export async function requireAuth(req) {
  const token = bearerToken(req) || cookieToken(req);
  if (!token) {
    const error = new Error("Sign in to save, replay, or export runs.");
    error.statusCode = 401;
    throw error;
  }
  try {
    return await verifyClerkToken(token, clerkRuntimeConfig());
  } catch (cause) {
    const error = new Error("Your sign-in session could not be verified.");
    error.statusCode = 401;
    error.cause = cause;
    throw error;
  }
}

export async function verifyClerkToken(token, config = clerkRuntimeConfig()) {
  if (!config.enabled) throw new Error("Clerk is not configured.");
  const [encodedHeader, encodedPayload, encodedSignature] = token.split(".");
  if (!encodedHeader || !encodedPayload || !encodedSignature) throw new Error("Invalid JWT.");
  const header = JSON.parse(base64urlDecode(encodedHeader).toString("utf8"));
  const payload = JSON.parse(base64urlDecode(encodedPayload).toString("utf8"));
  if (header.alg !== "RS256" || !header.kid) throw new Error("Unsupported JWT algorithm.");
  if (payload.iss !== config.issuer) throw new Error("Invalid token issuer.");
  const now = Math.floor(Date.now() / 1000);
  if (payload.exp && payload.exp < now) throw new Error("Token expired.");
  if (payload.nbf && payload.nbf > now) throw new Error("Token not active yet.");
  if (payload.sts === "pending") throw new Error("User registration is pending.");
  if (payload.azp && config.authorizedParties.length && !config.authorizedParties.includes(payload.azp)) {
    throw new Error("Token was issued for an unauthorized origin.");
  }
  const jwk = await findJwk(config.jwksUrl, header.kid);
  const key = createPublicKey({ key: jwk, format: "jwk" });
  const ok = verify(
    "RSA-SHA256",
    Buffer.from(`${encodedHeader}.${encodedPayload}`),
    key,
    base64urlDecode(encodedSignature),
  );
  if (!ok) throw new Error("Invalid token signature.");
  return {
    userId: payload.sub,
    sessionId: payload.sid,
    claims: payload,
  };
}

export function decodeClerkFrontendApi(publishableKey) {
  const encoded = publishableKey.split("_")[2];
  if (!encoded) return "";
  try {
    return normalizeClerkFrontendApi(base64urlDecode(encoded).toString("utf8").replace(/\$$/, ""));
  } catch {
    return "";
  }
}

function normalizeClerkFrontendApi(value) {
  const candidate = String(value || "").trim();
  if (!candidate || /[\x00-\x1f\x7f]/.test(candidate)) return "";
  try {
    const parsed = new URL(candidate.startsWith("http://") || candidate.startsWith("https://")
      ? candidate
      : `https://${candidate}`);
    const hostname = parsed.hostname;
    return /^[A-Za-z0-9.-]+$/.test(hostname) && hostname.includes(".") ? hostname : "";
  } catch {
    return "";
  }
}

async function findJwk(jwksUrl, kid) {
  const cached = jwksCache.get(jwksUrl);
  if (cached?.expiresAt > Date.now()) {
    const key = cached.keys.find(item => item.kid === kid);
    if (key) return key;
  }
  const response = await fetch(jwksUrl);
  if (!response.ok) throw new Error("Unable to fetch Clerk JWKS.");
  const body = await response.json();
  jwksCache.set(jwksUrl, {
    keys: body.keys || [],
    expiresAt: Date.now() + 60 * 60 * 1000,
  });
  const key = (body.keys || []).find(item => item.kid === kid);
  if (!key) throw new Error("Clerk signing key not found.");
  return key;
}

function bearerToken(req) {
  const header = req.headers.authorization || "";
  return header.toLowerCase().startsWith("bearer ") ? header.slice(7).trim() : "";
}

function cookieToken(req) {
  const cookie = req.headers.cookie || "";
  const match = cookie.match(/(?:^|;\s*)__session=([^;]+)/);
  return match ? decodeURIComponent(match[1]) : "";
}

function base64urlDecode(value) {
  const padded = value.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(value.length / 4) * 4, "=");
  return Buffer.from(padded, "base64");
}
