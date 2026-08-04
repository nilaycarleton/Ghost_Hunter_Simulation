# Session API

The web service runs each simulation as an isolated native process. Run metadata
and ordered events are persisted in SQLite before being exposed to clients.

## Authentication model

Guests can create live simulations without signing in. Guest runs are not
persisted after the native worker exits.

Signed-in users send a Clerk session token as a Bearer token. Those runs are
stored with the Clerk user id and are private to that account.

```http
Authorization: Bearer <clerk-session-token>
```

The browser dashboard gets Clerk configuration from:

```http
GET /api/auth/config
```

## Create a session

```http
POST /api/sessions
Content-Type: application/json

{
  "seed": 42,
  "scheduler": "deterministic",
  "navigation": "bfs",
  "hunters": ["Ada", "Grace"]
}
```

The response includes the UUID, SSE URL, replay URL, and `persisted`. When
`persisted` is `false`, the session is a guest live run.

## Stream or replay events

```http
GET /api/sessions/:id/events
Accept: text/event-stream
```

Active runs stream live `simulation` events. Completed runs replay their stored
events through the same endpoint, followed by a `done` event. Private
authenticated streams require the signed-in user's token. The browser passes the
token as an SSE query parameter because native `EventSource` does not support
custom request headers.

## Cancel a session

```http
DELETE /api/sessions/:id
```

Cancellation sends `SIGTERM` to the worker and records the run as cancelled.

## History and run details

```http
GET /api/runs?limit=20
GET /api/runs/:id
```

These endpoints require a signed-in user and return only that account's runs.

## Public sharing

Run history is private by default. A signed-in owner can make a completed run
public:

```http
POST /api/runs/:id/share
Authorization: Bearer <clerk-session-token>
```

The response includes `publicUrl`, such as `/share/abc123`. Anyone with that URL
can view the replay without signing in. Owners can revoke the public link:

```http
DELETE /api/runs/:id/share
Authorization: Bearer <clerk-session-token>
```

Public replay data is exposed through token-based endpoints:

```http
GET /api/shares/:token
GET /api/shares/:token/events
```

If you want to embed public share pages in your portfolio, add the portfolio
origin to `PUBLIC_FRAME_ANCESTORS`, for example
`PUBLIC_FRAME_ANCESTORS=https://hauntedthreads.site`.

## Free run explanations

The app includes a no-cost “Explain this run” panel. By default it uses the
local template provider:

```dotenv
AI_PROVIDER=template
```

Template mode analyzes the run/event stream and generates:

- ghost behavior summary;
- hunter outcomes;
- evidence collected;
- navigation explanation for BFS, breadcrumb, or random;
- interviewer-mode notes about concurrency, locking, memory, and debugging;
- benchmark narration.

Saved private runs can be explained through:

```http
GET /api/runs/:id/explain?audience=recruiter&mode=summary
Authorization: Bearer <clerk-session-token>
```

Public shared runs can be explained without signing in:

```http
GET /api/shares/:token/explain?audience=engineer&mode=benchmark
```

`audience` accepts `recruiter`, `engineer`, or `beginner`. `mode` accepts
`summary`, `interviewer`, or `benchmark`.

For local development only, you can optionally use a local Ollama model:

```dotenv
AI_PROVIDER=ollama
OLLAMA_URL=http://127.0.0.1:11434
OLLAMA_MODEL=llama3.2
```

If Ollama is unavailable or times out, the app falls back to template mode.

## Export

```http
GET /api/runs/:id/export?format=json
GET /api/runs/:id/export?format=csv
```

## Health

```http
GET /api/health
```

Session creation is rate-limited per client address. The default server admits
four concurrent workers and terminates any worker exceeding 60 seconds. These
limits can be changed with `MAX_CONCURRENT_SESSIONS` and
`SESSION_TIMEOUT_MS`.
