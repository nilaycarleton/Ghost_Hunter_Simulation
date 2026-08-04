import { createServer } from "node:http";
import { randomBytes } from "node:crypto";
import { createReadStream, existsSync } from "node:fs";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { clerkRuntimeConfig, optionalAuth, requireAuth } from "./auth.mjs";
import { RunDatabase } from "./database.mjs";
import { explainRun } from "./explanations.mjs";
import { SessionManager } from "./sessions.mjs";

if (existsSync(".env")) process.loadEnvFile?.(".env");

const root = normalize(fileURLToPath(new URL("..", import.meta.url)));
const web = join(root, "web");
const port = Number(process.env.PORT || 3000);
const host = process.env.HOST || "127.0.0.1";
const types = { ".html": "text/html", ".css": "text/css", ".js": "text/javascript" };
const database = new RunDatabase(process.env.DATABASE_PATH || join(root, "data", "haunted-threads.db"));
const sessions = new SessionManager({
  root, database,
  maximum: Number(process.env.MAX_CONCURRENT_SESSIONS || 4),
  timeoutMs: Number(process.env.SESSION_TIMEOUT_MS || 60_000),
});
const rateBuckets = new Map();
const clerkAssetCache = new Map();

const server = createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  setCommonHeaders(res);
  try {
    if (url.pathname === "/api/health" && req.method === "GET") {
      return json(res, 200, {
        status: "ok",
        activeSessions: sessions.active.size,
        auth: clerkRuntimeConfig().enabled ? "enabled" : "guest-only",
      });
    }

    if (url.pathname === "/api/auth/config" && req.method === "GET") {
      const config = clerkRuntimeConfig();
      return json(res, 200, {
        enabled: config.enabled,
        publishableKey: config.publishableKey,
        frontendApi: config.frontendApi,
      });
    }

    if (url.pathname.startsWith("/assets/ghlab/") && req.method === "GET") {
      return serveClerkAsset(url, res);
    }

    if (url.pathname === "/api/sessions" && req.method === "POST") {
      if (!allowRequest(req)) return json(res, 429, { error: "Too many simulation requests. Try again in one minute." });
      const user = await optionalAuth(req);
      const config = validateConfig(await readJson(req));
      const session = sessions.create(config, { persist: Boolean(user), ownerId: user?.userId });
      return json(res, 201, session);
    }

    const sessionEvents = url.pathname.match(/^\/api\/sessions\/([a-f0-9-]+)\/events$/);
    if (sessionEvents && req.method === "GET") {
      applyTokenQuery(req, url);
      await authorizeSession(req, sessionEvents[1]);
      res.writeHead(200, {
        "content-type": "text/event-stream",
        "cache-control": "no-cache, no-transform",
        "connection": "keep-alive",
      });
      res.write(": connected\n\n");
      if (!sessions.subscribe(sessionEvents[1], res)) {
        res.write(`event: error\ndata: ${JSON.stringify({ error: "Session not found." })}\n\n`);
        res.end();
      }
      return;
    }

    const sessionRoute = url.pathname.match(/^\/api\/sessions\/([a-f0-9-]+)$/);
    if (sessionRoute && req.method === "DELETE") {
      await authorizeSession(req, sessionRoute[1], { activeOnly: true });
      return sessions.cancel(sessionRoute[1])
        ? json(res, 202, { status: "cancelling" })
        : json(res, 404, { error: "Active session not found." });
    }

    if (url.pathname === "/api/runs" && req.method === "GET") {
      const user = await requireAuth(req);
      const limit = Number(url.searchParams.get("limit") || 20);
      return json(res, 200, { runs: database.list(Number.isFinite(limit) ? limit : 20, user.userId) });
    }

    const shareRoute = url.pathname.match(/^\/api\/runs\/([a-f0-9-]+)\/share$/);
    if (shareRoute && req.method === "POST") {
      const run = await authorizeRun(req, shareRoute[1]);
      if (run.status === "running") return json(res, 409, { error: "Wait until the run finishes before making it public." });
      const shared = database.share(run.id, run.shareToken || createShareToken(), new Date().toISOString());
      return json(res, 200, { run: shared, publicUrl: shared.publicUrl });
    }

    if (shareRoute && req.method === "DELETE") {
      const run = await authorizeRun(req, shareRoute[1]);
      const revoked = database.revokeShare(run.id);
      return json(res, 200, { run: revoked });
    }

    const publicShareEvents = url.pathname.match(/^\/api\/shares\/([A-Za-z0-9_-]+)\/events$/);
    if (publicShareEvents && req.method === "GET") {
      const run = database.getByShareToken(publicShareEvents[1]);
      if (!run) return json(res, 404, { error: "Shared run not found." });
      res.writeHead(200, {
        "content-type": "text/event-stream",
        "cache-control": "no-cache, no-transform",
        "connection": "keep-alive",
      });
      for (const event of database.getEvents(run.id)) {
        res.write(sse("simulation", event, event.sequence));
      }
      res.write(sse("done", {
        id: run.id, status: run.status, exitCode: run.exitCode,
        eventCount: run.eventCount, summary: run.summary, error: run.error,
      }));
      return res.end();
    }

    const publicShare = url.pathname.match(/^\/api\/shares\/([A-Za-z0-9_-]+)$/);
    if (publicShare && req.method === "GET") {
      const run = database.getByShareToken(publicShare[1]);
      return run ? json(res, 200, run) : json(res, 404, { error: "Shared run not found." });
    }

    const publicShareExplain = url.pathname.match(/^\/api\/shares\/([A-Za-z0-9_-]+)\/explain$/);
    if (publicShareExplain && req.method === "GET") {
      const run = database.getByShareToken(publicShareExplain[1]);
      if (!run) return json(res, 404, { error: "Shared run not found." });
      const explanation = await explainRun({
        run,
        events: database.getEvents(run.id),
        audience: url.searchParams.get("audience"),
        mode: url.searchParams.get("mode"),
      });
      return json(res, 200, explanation);
    }

    const exportRoute = url.pathname.match(/^\/api\/runs\/([a-f0-9-]+)\/export$/);
    if (exportRoute && req.method === "GET") {
      const run = await authorizeRun(req, exportRoute[1]);
      const events = database.getEvents(run.id);
      if (url.searchParams.get("format") === "csv") {
        const rows = ["sequence,timestamp_us,entity,id,room,device,boredom,fear,action,extra"];
        for (const event of events) {
          rows.push([
            event.sequence,event.timestamp_us,event.entity,event.id,event.room,event.device,
            event.boredom,event.fear,event.action,event.extra,
          ].map(csvCell).join(","));
        }
        res.writeHead(200, {
          "content-type": "text/csv; charset=utf-8",
          "content-disposition": `attachment; filename="haunted-threads-run-${run.id}.csv"`,
        });
        return res.end(`${rows.join("\n")}\n`);
      }
      res.writeHead(200, {
        "content-type": "application/json; charset=utf-8",
        "content-disposition": `attachment; filename="haunted-threads-run-${run.id}.json"`,
      });
      return res.end(JSON.stringify({ run, events }, null, 2));
    }

    const runRoute = url.pathname.match(/^\/api\/runs\/([a-f0-9-]+)$/);
    if (runRoute && req.method === "GET") {
      const run = await authorizeRun(req, runRoute[1]);
      return json(res, 200, run);
    }

    const runExplainRoute = url.pathname.match(/^\/api\/runs\/([a-f0-9-]+)\/explain$/);
    if (runExplainRoute && req.method === "GET") {
      const run = await authorizeRun(req, runExplainRoute[1]);
      const explanation = await explainRun({
        run,
        events: database.getEvents(run.id),
        audience: url.searchParams.get("audience"),
        mode: url.searchParams.get("mode"),
      });
      return json(res, 200, explanation);
    }

    if (req.method !== "GET") return json(res, 404, { error: "Not found." });
    const replayRoute = url.pathname.match(/^\/replay\/([a-f0-9-]+)$/);
    const publicShareRoute = url.pathname.match(/^\/share\/([A-Za-z0-9_-]+)$/);
    const relative = url.pathname === "/" || replayRoute || publicShareRoute
      ? "index.html" : url.pathname.slice(1);
    const file = normalize(join(web, relative));
    if (!file.startsWith(web) || !existsSync(file)) {
      res.writeHead(404).end("Not found");
      return;
    }
    res.writeHead(200, {
      "content-type": types[extname(file)] || "application/octet-stream",
      "content-security-policy": contentSecurityPolicy(),
    });
    createReadStream(file).pipe(res);
  } catch (error) {
    const status = error.statusCode || 500;
    json(res, status, { error: status === 500 ? "Internal server error." : error.message });
    if (status === 500) console.error(error);
  }
});

server.listen(port, host, () => {
  console.log(`Haunted Threads: http://${host}:${port}`);
});

function validateConfig(input = {}) {
  const seed = Number(input.seed ?? 42);
  const navigation = input.navigation ?? "bfs";
  const scheduler = input.scheduler ?? "threads";
  const hunters = Array.isArray(input.hunters)
    ? input.hunters.map(name => String(name).trim()).filter(Boolean) : [];
  const valid = Number.isInteger(seed) && seed >= 0 && seed <= 4294967295
    && ["bfs","breadcrumb","random"].includes(navigation)
    && ["threads","deterministic"].includes(scheduler)
    && hunters.length >= 1 && hunters.length <= 8
    && hunters.every(name => /^[A-Za-z0-9 _'-]{1,63}$/.test(name));
  if (!valid) {
    const error = new Error("Invalid seed, scheduler, navigation strategy, or hunter team.");
    error.statusCode = 400;
    throw error;
  }
  return { seed, navigation, scheduler, hunters };
}

function readJson(req) {
  return new Promise((resolve, reject) => {
    let body = "";
    req.on("data", chunk => {
      body += chunk;
      if (body.length > 16_384) {
        const error = new Error("Request body is too large.");
        error.statusCode = 413;
        reject(error);
        req.destroy();
      }
    });
    req.on("end", () => {
      try { resolve(body ? JSON.parse(body) : {}); }
      catch {
        const error = new Error("Request body must be valid JSON.");
        error.statusCode = 400;
        reject(error);
      }
    });
    req.on("error", reject);
  });
}

function allowRequest(req) {
  const key = req.socket.remoteAddress || "unknown";
  const now = Date.now();
  const recent = (rateBuckets.get(key) || []).filter(time => now - time < 60_000);
  if (recent.length >= 10) return false;
  recent.push(now);
  rateBuckets.set(key, recent);
  return true;
}

function setCommonHeaders(res) {
  res.setHeader("x-content-type-options", "nosniff");
  res.setHeader("referrer-policy", "no-referrer");
}

function applyTokenQuery(req, url) {
  const token = url.searchParams.get("token");
  if (token && !req.headers.authorization) {
    req.headers.authorization = `Bearer ${token}`;
  }
}

async function authorizeSession(req, id, options = {}) {
  const session = sessions.get(id);
  if (session) {
    if (!session.ownerId) return null;
    const user = await requireAuth(req);
    if (user.userId === session.ownerId) return user;
    const error = new Error("This simulation belongs to another account.");
    error.statusCode = 403;
    throw error;
  }
  if (options.activeOnly) return null;
  await authorizeRun(req, id);
  return null;
}

async function authorizeRun(req, id) {
  const user = await requireAuth(req);
  const run = database.get(id);
  if (!run) {
    const error = new Error("Run not found.");
    error.statusCode = 404;
    throw error;
  }
  if (run.ownerId !== user.userId) {
    const error = new Error("This run belongs to another account.");
    error.statusCode = 403;
    throw error;
  }
  return run;
}

function contentSecurityPolicy() {
  const config = clerkRuntimeConfig();
  const origins = ["'self'"];
  if (config.frontendApi) origins.push(`https://${config.frontendApi}`);
  return [
    "default-src 'self'",
    "style-src 'self' 'unsafe-inline'",
    `script-src ${origins.join(" ")} 'unsafe-inline'`,
    `connect-src ${origins.join(" ")}`,
    `frame-src ${origins.join(" ")}`,
    `frame-ancestors ${frameAncestors()}`,
    "img-src 'self' data: https:",
  ].join("; ");
}

function frameAncestors() {
  const configured = (process.env.PUBLIC_FRAME_ANCESTORS || "")
    .split(",").map(origin => origin.trim()).filter(Boolean);
  return ["'self'", ...configured].join(" ");
}

function createShareToken() {
  return randomBytes(18).toString("base64url");
}

function sse(type, data, id) {
  return `${id ? `id: ${id}\n` : ""}event: ${type}\ndata: ${JSON.stringify(data)}\n\n`;
}

function json(res, status, value) {
  if (res.writableEnded) return;
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(value));
}

async function serveClerkAsset(url, res) {
  const config = clerkRuntimeConfig();
  if (!config.frontendApi) return json(res, 404, { error: "Clerk is not configured." });
  const assetPath = clerkAssetPath(url.pathname);
  if (!assetPath) return json(res, 404, { error: "Clerk asset not found." });
  const cacheKey = `${config.frontendApi}${url.pathname}`;
  const cached = clerkAssetCache.get(cacheKey);
  if (cached?.expiresAt > Date.now()) {
    res.writeHead(200, cached.headers);
    return res.end(cached.body);
  }
  let upstream = await fetch(`https://${config.frontendApi}${assetPath}`);
  if (!upstream.ok && url.pathname.startsWith("/assets/ghlab/")) {
    upstream = await fetch(`https://${config.frontendApi}/npm/@clerk/ui@1/dist/${url.pathname.split("/").pop()}`);
  }
  if (!upstream.ok) return json(res, 502, { error: "Unable to load Clerk asset." });
  const contentType = upstream.headers.get("content-type") || "application/javascript; charset=utf-8";
  const body = Buffer.from(await upstream.arrayBuffer());
  const headers = {
    "content-type": contentType,
    "cache-control": "public, max-age=3600",
  };
  clerkAssetCache.set(cacheKey, { body, headers, expiresAt: Date.now() + 60 * 60 * 1000 });
  res.writeHead(200, headers);
  res.end(body);
}

function clerkAssetPath(pathname) {
  if (pathname === "/assets/ghlab/ui.js") return "/npm/@clerk/ui@1/dist/ui.browser.js";
  if (pathname === "/assets/ghlab/sdk.js") return "/npm/@clerk/clerk-js@6/dist/clerk.browser.js";
  if (pathname.startsWith("/assets/ghlab/")) {
    const filename = pathname.split("/").pop();
    if (/^[A-Za-z0-9._-]+\.js$/.test(filename)) return `/npm/@clerk/clerk-js@6/dist/${filename}`;
  }
  return "";
}

function csvCell(value) {
  const text = value == null ? "" : String(value);
  return /[",\n]/.test(text) ? `"${text.replaceAll('"','""')}"` : text;
}

function shutdown() {
  sessions.shutdown();
  server.close(() => {
    database.close();
    process.exit(0);
  });
  setTimeout(() => process.exit(1), 3000).unref();
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
