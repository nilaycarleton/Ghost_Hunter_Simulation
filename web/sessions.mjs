import { spawn } from "node:child_process";
import { randomUUID } from "node:crypto";
import { mkdirSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";

export class SessionManager {
  constructor({ root, database, maximum = 4, timeoutMs = 60_000 }) {
    this.root = root;
    this.database = database;
    this.maximum = maximum;
    this.timeoutMs = timeoutMs;
    this.active = new Map();
    mkdirSync(join(root, "data"), { recursive: true });
  }

  create(config, options = {}) {
    if (this.active.size >= this.maximum) {
      const error = new Error("The simulation server is at capacity. Try again shortly.");
      error.statusCode = 503;
      throw error;
    }
    const id = randomUUID();
    const summaryPath = join(this.root, "data", `${id}.json`);
    const session = {
      id, config, summaryPath, events: [], subscribers: new Set(),
      createdAt: new Date().toISOString(), child: null, cancelled: false,
      stderr: "", timer: null, ownerId: options.ownerId || null,
      persist: options.persist !== false,
    };
    if (session.persist) {
      this.database.create({ id, ownerId: session.ownerId, createdAt: session.createdAt, ...config });
    }
    this.active.set(id, session);
    this.#start(session);
    return publicSession(session);
  }

  #start(session) {
    const { config } = session;
    const args = [
      "--seed", String(config.seed),
      "--navigation", config.navigation,
      "--hunters", config.hunters.join(","),
      "--output-json", session.summaryPath,
    ];
    if (config.scheduler === "deterministic") args.push("--deterministic");
    session.child = spawn(join(this.root, "ghost_hunt"), args, {
      cwd: this.root,
      env: { ...process.env, GH_JSON_EVENTS: "1" },
      stdio: ["ignore", "pipe", "pipe"],
    });
    session.timer = setTimeout(() => {
      session.cancelled = true;
      session.stderr = "Simulation exceeded the server time limit.";
      session.child.kill("SIGTERM");
    }, this.timeoutMs);

    let pending = "";
    session.child.stdout.on("data", chunk => {
      pending += chunk;
      const lines = pending.split("\n");
      pending = lines.pop();
      for (const line of lines) {
        if (!line.startsWith("EVENT ")) continue;
        try {
          const event = JSON.parse(line.slice(6));
          session.events.push(event);
          if (session.persist) this.database.appendEvent(session.id, event);
          this.#broadcast(session, "simulation", event, event.sequence);
        } catch {
          session.stderr = "The native engine emitted an invalid event.";
        }
      }
    });
    session.child.stderr.on("data", chunk => {
      session.stderr = (session.stderr + chunk).slice(-4000);
    });
    session.child.on("error", error => {
      session.stderr = error.message;
    });
    session.child.on("close", code => this.#complete(session, code));
  }

  #complete(session, code) {
    clearTimeout(session.timer);
    let summary = null;
    try {
      summary = JSON.parse(readFileSync(session.summaryPath, "utf8"));
    } catch {}
    rmSync(session.summaryPath, { force: true });
    const status = session.cancelled ? "cancelled" : code === 0 ? "completed" : "failed";
    const result = {
      status,
      completedAt: new Date().toISOString(),
      exitCode: code,
      eventCount: session.events.length,
      summary,
      error: session.stderr.trim() || null,
    };
    if (session.persist) this.database.finish(session.id, result);
    this.#broadcast(session, "done", { id: session.id, ...result });
    for (const response of session.subscribers) response.end();
    session.subscribers.clear();
    this.active.delete(session.id);
  }

  #broadcast(session, type, data, id) {
    const message = sse(type, data, id);
    for (const response of session.subscribers) response.write(message);
  }

  subscribe(id, response) {
    const session = this.active.get(id);
    if (session) {
      for (const event of session.events) {
        response.write(sse("simulation", event, event.sequence));
      }
      session.subscribers.add(response);
      response.on("close", () => session.subscribers.delete(response));
      return true;
    }
    const run = this.database.get(id);
    if (!run) return false;
    for (const event of this.database.getEvents(id)) {
      response.write(sse("simulation", event, event.sequence));
    }
    response.write(sse("done", {
      id, status: run.status, exitCode: run.exitCode,
      eventCount: run.eventCount, summary: run.summary, error: run.error,
    }));
    response.end();
    return true;
  }

  get(id) {
    return this.active.get(id) || null;
  }

  cancel(id) {
    const session = this.active.get(id);
    if (!session) return false;
    session.cancelled = true;
    session.stderr = "Cancelled by client.";
    session.child.kill("SIGTERM");
    return true;
  }

  shutdown() {
    for (const session of this.active.values()) {
      session.cancelled = true;
      session.child.kill("SIGTERM");
    }
  }
}

function publicSession(session) {
  return {
    id: session.id,
    status: "running",
    createdAt: session.createdAt,
    ...session.config,
    eventsUrl: `/api/sessions/${session.id}/events`,
    replayUrl: `/replay/${session.id}`,
    persisted: session.persist,
  };
}

function sse(type, data, id) {
  return `${id ? `id: ${id}\n` : ""}event: ${type}\ndata: ${JSON.stringify(data)}\n\n`;
}
