# Haunted Threads Render deployment runbook

This runbook deploys Haunted Threads to Render Free at:

```text
https://hauntedthreads.site
https://www.hauntedthreads.site
```

Render is a good project/demo target for this app because it can run a dynamic
Node web service and compile the C simulation binary during build. The tradeoff
is that Render Free uses ephemeral storage, so SQLite-backed saved runs and
public share links should be treated as temporary demo data.

## 1. Commit and push first

Render deploys from GitHub. Before creating the Render service, make sure the
repository includes:

- C source files and `Makefile`
- `package.json`
- `web/` and `web/legal/`
- `docs/`
- `tests/`, `scripts/`, `benchmarks/`, and `.github/`
- `.gitignore` and `.env.example`

Do not commit `.env`.

## 2. Create the Render service

In the Render dashboard:

1. Click **New**.
2. Select **Web Services**.
3. Connect `nilaycarleton/Ghost_Hunter_Simulation`.
4. Select branch `main`.
5. Use these settings:

```text
Name: haunted-threads
Runtime: Node
Instance Type: Free
Build Command: npm run build
Start Command: npm start
Health Check Path: /api/health
```

Render sets `PORT=10000` by default for web services. The app must bind to
`0.0.0.0` in production.

## 3. Environment variables

Add these Render environment variables:

```dotenv
NODE_ENV=production
HOST=0.0.0.0
PORT=10000
DATABASE_PATH=data/haunted-threads.db
MAX_CONCURRENT_SESSIONS=2
SESSION_TIMEOUT_MS=60000
AI_PROVIDER=template
```

If sign-in, saved history, replay ownership, and exports should work, also add
Clerk values:

```dotenv
CLERK_PUBLISHABLE_KEY=pk_live_or_test_value
CLERK_SECRET_KEY=sk_live_or_test_value
CLERK_AUTHORIZED_PARTIES=https://hauntedthreads.site,https://www.hauntedthreads.site
PUBLIC_FRAME_ANCESTORS=https://hauntedthreads.site,https://www.hauntedthreads.site
```

Use Render's environment variable UI for secrets. Do not put real keys in the
repository.

## 4. Test the Render subdomain first

Before changing Namecheap DNS, test the generated Render URL:

```text
https://<your-service-name>.onrender.com
```

Checklist:

- `/api/health` returns `status: ok`.
- Homepage loads.
- Guest simulation starts.
- Legal footer pages load.
- Mobile layout works.
- Light/dark mode persists.

Cold starts are expected on Render Free after idle periods.

## 5. Add custom domains in Render

In the Render service settings:

1. Open **Custom Domains**.
2. Add `hauntedthreads.site`.
3. Render will show the DNS records required for the root and `www` domain.
4. Keep this page open while editing Namecheap DNS.

## 6. Update Namecheap DNS

In Namecheap Advanced DNS for `hauntedthreads.site`, remove the current parking
and redirect records:

- `CNAME Record` for `www` pointing to `parkingpage.namecheap.com`
- `URL Redirect Record` for `@`
- Any `AAAA` records if present

Then add exactly the DNS records Render provides for:

- `hauntedthreads.site`
- `www.hauntedthreads.site`

Do not guess the record target before Render shows it. Return to Render and
click **Verify** after saving DNS.

## 7. Final production smoke test

After Render verifies the custom domain and issues TLS certificates, test:

- `https://hauntedthreads.site`
- `https://www.hauntedthreads.site`
- HTTP redirects to HTTPS.
- `/api/health` returns `status: ok`.
- Guest simulation starts.
- Sign-in opens Clerk, if Clerk is enabled.
- Signed-in run saves to history, with the Render Free storage caveat.
- Replay links work while data exists.
- Public share links work while data exists.
- JSON and CSV exports work for signed-in runs.
- Legal footer links return `200`.

## 8. Known Render Free limitations

- The service sleeps after idle time.
- The first request after sleep can be slow.
- Local SQLite files are not durable across restarts or redeploys.
- Saved history and public shares are suitable for demo usage only.

For reliable persistence, move to one of:

- Render paid web service with persistent disk
- Railway with a volume
- Fly.io with a volume
- External Postgres storage
