# Haunted Threads Render production checklist

Use this checklist before announcing the public demo at `hauntedthreads.site`.

## Repository

- [ ] `main` includes the C backend, Node web server, frontend, tests, docs, and legal pages.
- [ ] `.env` is not committed.
- [ ] `.gitignore` excludes build outputs, local databases, backups, and secrets.
- [ ] `npm run build` succeeds locally.
- [ ] `npm test` succeeds locally.

## Render service

- [ ] Service type is **Web Service**.
- [ ] Repository is `nilaycarleton/Ghost_Hunter_Simulation`.
- [ ] Branch is `main`.
- [ ] Runtime is Node.
- [ ] Instance type is Free.
- [ ] Build command is `npm run build`.
- [ ] Start command is `npm start`.
- [ ] Health check path is `/api/health`.

## Render environment

- [ ] `NODE_ENV=production`.
- [ ] `HOST=0.0.0.0`.
- [ ] `PORT=10000`.
- [ ] `DATABASE_PATH=data/haunted-threads.db`.
- [ ] `MAX_CONCURRENT_SESSIONS=2`.
- [ ] `SESSION_TIMEOUT_MS=60000`.
- [ ] `AI_PROVIDER=template`.
- [ ] `CLERK_PUBLISHABLE_KEY` is set if auth is enabled.
- [ ] `CLERK_SECRET_KEY` is set if auth is enabled.
- [ ] `CLERK_AUTHORIZED_PARTIES` includes `https://hauntedthreads.site`.
- [ ] `CLERK_AUTHORIZED_PARTIES` includes `https://www.hauntedthreads.site`.
- [ ] `PUBLIC_FRAME_ANCESTORS` includes both production domains.

## Render subdomain smoke test

- [ ] `https://<service>.onrender.com/api/health` returns `status: ok`.
- [ ] Homepage loads.
- [ ] Guest mode can run a simulation.
- [ ] Legal footer links load.
- [ ] Mobile layout works.
- [ ] Light/dark mode persists.

## Namecheap DNS

- [ ] Remove Namecheap parking `CNAME` for `www`.
- [ ] Remove Namecheap URL redirect record for `@`.
- [ ] Remove any `AAAA` records if present.
- [ ] Add the root-domain DNS record Render provides for `hauntedthreads.site`.
- [ ] Add the `www` DNS record Render provides for `www.hauntedthreads.site`.
- [ ] Render verifies the custom domain.
- [ ] Render provisions TLS.

## Clerk

- [ ] Clerk dashboard allows `https://hauntedthreads.site`.
- [ ] Clerk dashboard allows `https://www.hauntedthreads.site`.
- [ ] Sign-in opens from the production domain.
- [ ] Signed-in run is saved to history, with Render Free storage caveat.

## Product smoke test

- [ ] Guest mode can run a simulation.
- [ ] Sign-in opens Clerk, if auth is enabled.
- [ ] Signed-in run is saved to history, if auth is enabled.
- [ ] Replay works while data exists.
- [ ] Make public creates a `/share/:token` URL.
- [ ] Public share URL works while signed out.
- [ ] "Explain this run" works in template mode.
- [ ] JSON and CSV export work for signed-in runs.
- [ ] Header navigation links work.
- [ ] Footer legal links return `200`.

## Render Free caveats

- [ ] Cold starts are acceptable for this project demo.
- [ ] Saved SQLite run data is treated as temporary.
- [ ] Public share links are treated as temporary.
- [ ] A future persistence upgrade is documented if durable history is required.
