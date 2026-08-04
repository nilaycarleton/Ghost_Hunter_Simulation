import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { RunDatabase } from "../web/database.mjs";
import { explainRun } from "../web/explanations.mjs";
import { SessionManager } from "../web/sessions.mjs";

const temporary = mkdtempSync(join(tmpdir(), "ghost-hunter-test-"));
const database = new RunDatabase(join(temporary, "runs.db"));
const manager = new SessionManager({
  root: resolve(import.meta.dirname, ".."),
  database,
  maximum: 3,
  timeoutMs: 5_000,
});

const config = {
  seed: 77,
  navigation: "bfs",
  scheduler: "deterministic",
  hunters: ["Ada", "Grace"],
};

try {
  const first = manager.create(config);
  const second = manager.create({ ...config, seed: 78, navigation: "random" });
  await waitFor(() => database.get(first.id)?.status !== "running");
  await waitFor(() => database.get(second.id)?.status !== "running");

  const stored = database.get(first.id);
  assert.equal(stored.status, "completed", JSON.stringify(stored));
  assert.equal(stored.seed, 77);
  assert.equal(stored.visibility, "private");
  assert.equal(stored.shareToken, null);
  assert.ok(stored.eventCount > 0);
  assert.equal(database.getEvents(first.id).length, stored.eventCount);
  assert.equal(stored.summary.scheduler, "deterministic");
  const shared = database.share(first.id, "test-public-token", new Date().toISOString());
  assert.equal(shared.visibility, "public");
  assert.equal(shared.shareToken, "test-public-token");
  assert.equal(database.getByShareToken("test-public-token").id, first.id);
  const privateAgain = database.revokeShare(first.id);
  assert.equal(privateAgain.visibility, "private");
  assert.equal(privateAgain.shareToken, null);
  assert.equal(database.getByShareToken("test-public-token"), null);
  const explanation = await explainRun({
    run: stored,
    events: database.getEvents(first.id),
    audience: "engineer",
    mode: "interviewer",
  });
  assert.equal(explanation.provider, "template");
  assert.ok(explanation.cards.length >= 4);
  assert.match(explanation.sections.locking, /Canonical locking/);
  const friendlyExplanation = await explainRun({
    run: stored,
    events: database.getEvents(first.id),
  });
  assert.equal(friendlyExplanation.audience, "beginner");
  assert.equal(friendlyExplanation.title, "What happened in this run");
  assert.match(friendlyExplanation.sections["The story"], /shortest-path route plan/);

  const cancelled = manager.create({ ...config, scheduler: "threads", seed: 79 });
  assert.equal(manager.cancel(cancelled.id), true);
  await waitFor(() => database.get(cancelled.id)?.status !== "running");
  assert.equal(database.get(cancelled.id).status, "cancelled");

  assert.equal(database.list(10).length, 3);
  const owned = manager.create({ ...config, seed: 80 }, { ownerId: "user_test_123" });
  await waitFor(() => database.get(owned.id)?.status !== "running");
  assert.equal(database.get(owned.id).ownerId, "user_test_123");
  assert.equal(database.list(10, "user_test_123").length, 1);
  assert.equal(database.list(10, "missing_user").length, 0);

  console.log("Session tests passed: concurrent workers, persistence, events, cancellation, ownership, and sharing.");
} finally {
  manager.shutdown();
  database.close();
  rmSync(temporary, { recursive: true, force: true });
}

async function waitFor(predicate) {
  const deadline = Date.now() + 5_000;
  while (!predicate()) {
    if (Date.now() > deadline) throw new Error("Timed out waiting for session.");
    await new Promise(resolve => setTimeout(resolve, 20));
  }
}
