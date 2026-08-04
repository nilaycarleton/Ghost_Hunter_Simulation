import { DatabaseSync } from "node:sqlite";
import { mkdirSync } from "node:fs";
import { dirname } from "node:path";

export class RunDatabase {
  constructor(path) {
    mkdirSync(dirname(path), { recursive: true });
    this.db = new DatabaseSync(path);
    this.db.exec(`
      PRAGMA journal_mode = WAL;
      PRAGMA foreign_keys = ON;
      CREATE TABLE IF NOT EXISTS runs (
        id TEXT PRIMARY KEY,
        owner_id TEXT,
        status TEXT NOT NULL,
        created_at TEXT NOT NULL,
        completed_at TEXT,
        seed INTEGER NOT NULL,
        navigation TEXT NOT NULL,
        scheduler TEXT NOT NULL,
        hunters_json TEXT NOT NULL,
        exit_code INTEGER,
        event_count INTEGER NOT NULL DEFAULT 0,
        summary_json TEXT,
        error TEXT,
        visibility TEXT NOT NULL DEFAULT 'private',
        share_token TEXT UNIQUE,
        shared_at TEXT
      );
      CREATE TABLE IF NOT EXISTS events (
        run_id TEXT NOT NULL REFERENCES runs(id) ON DELETE CASCADE,
        sequence INTEGER NOT NULL,
        event_json TEXT NOT NULL,
        PRIMARY KEY (run_id, sequence)
      );
      CREATE INDEX IF NOT EXISTS runs_created_at ON runs(created_at DESC);
    `);
    this.#migrate();
    this.db.exec("CREATE INDEX IF NOT EXISTS runs_owner_created_at ON runs(owner_id, created_at DESC)");
    this.db.exec("CREATE UNIQUE INDEX IF NOT EXISTS runs_share_token ON runs(share_token)");
    this.insertRun = this.db.prepare(`
      INSERT INTO runs
        (id,owner_id,status,created_at,seed,navigation,scheduler,hunters_json)
      VALUES (?,?,?,?,?,?,?,?)
    `);
    this.insertEvent = this.db.prepare(
      "INSERT OR REPLACE INTO events (run_id,sequence,event_json) VALUES (?,?,?)"
    );
    this.finishRun = this.db.prepare(`
      UPDATE runs SET status=?,completed_at=?,exit_code=?,event_count=?,
        summary_json=?,error=? WHERE id=?
    `);
  }

  create(run) {
    this.insertRun.run(
      run.id, run.ownerId || null, "running", run.createdAt, run.seed, run.navigation,
      run.scheduler, JSON.stringify(run.hunters)
    );
  }

  appendEvent(runId, event) {
    this.insertEvent.run(runId, event.sequence, JSON.stringify(event));
  }

  finish(runId, result) {
    this.finishRun.run(
      result.status, result.completedAt, result.exitCode, result.eventCount,
      result.summary ? JSON.stringify(result.summary) : null,
      result.error || null, runId
    );
  }

  list(limit = 20, ownerId) {
    const safeLimit = Math.min(Math.max(limit, 1), 100);
    if (ownerId) {
      return this.db.prepare(`
        SELECT id,owner_id,status,created_at,completed_at,seed,navigation,scheduler,
          hunters_json,exit_code,event_count,summary_json,error,visibility,share_token,shared_at
        FROM runs WHERE owner_id=? ORDER BY created_at DESC LIMIT ?
      `).all(ownerId, safeLimit).map(normalizeRun);
    }
    return this.db.prepare(`
      SELECT id,status,created_at,completed_at,seed,navigation,scheduler,
        hunters_json,exit_code,event_count,summary_json,error,visibility,share_token,shared_at
      FROM runs ORDER BY created_at DESC LIMIT ?
    `).all(safeLimit).map(normalizeRun);
  }

  get(id) {
    const row = this.db.prepare("SELECT * FROM runs WHERE id=?").get(id);
    return row ? normalizeRun(row) : null;
  }

  getByShareToken(token) {
    const row = this.db.prepare(
      "SELECT * FROM runs WHERE visibility='public' AND share_token=?"
    ).get(token);
    return row ? normalizeRun(row) : null;
  }

  share(id, token, sharedAt) {
    this.db.prepare(`
      UPDATE runs SET visibility='public',share_token=?,shared_at=?
      WHERE id=?
    `).run(token, sharedAt, id);
    return this.get(id);
  }

  revokeShare(id) {
    this.db.prepare(`
      UPDATE runs SET visibility='private',share_token=NULL,shared_at=NULL
      WHERE id=?
    `).run(id);
    return this.get(id);
  }

  getEvents(id) {
    return this.db.prepare(
      "SELECT event_json FROM events WHERE run_id=? ORDER BY sequence"
    ).all(id).map(row => JSON.parse(row.event_json));
  }

  close() {
    this.db.close();
  }

  #migrate() {
    const columns = new Set(this.db.prepare("PRAGMA table_info(runs)").all().map(column => column.name));
    if (!columns.has("owner_id")) this.db.exec("ALTER TABLE runs ADD COLUMN owner_id TEXT");
    if (!columns.has("visibility")) this.db.exec("ALTER TABLE runs ADD COLUMN visibility TEXT NOT NULL DEFAULT 'private'");
    if (!columns.has("share_token")) this.db.exec("ALTER TABLE runs ADD COLUMN share_token TEXT");
    if (!columns.has("shared_at")) this.db.exec("ALTER TABLE runs ADD COLUMN shared_at TEXT");
  }
}

function normalizeRun(row) {
  return {
    id: row.id,
    ownerId: row.owner_id || null,
    status: row.status,
    createdAt: row.created_at,
    completedAt: row.completed_at,
    seed: row.seed,
    navigation: row.navigation,
    scheduler: row.scheduler,
    hunters: JSON.parse(row.hunters_json),
    exitCode: row.exit_code,
    eventCount: row.event_count,
    summary: row.summary_json ? JSON.parse(row.summary_json) : null,
    error: row.error,
    visibility: row.visibility || "private",
    shareToken: row.share_token || null,
    sharedAt: row.shared_at || null,
    publicUrl: row.share_token ? `/share/${row.share_token}` : null,
  };
}
