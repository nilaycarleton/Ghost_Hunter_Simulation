# Haunted Threads Oracle Cloud deployment runbook

This runbook deploys Haunted Threads to an Oracle Cloud Always Free Ubuntu VM
at `https://hauntedthreads.site`.

The app is intentionally small: one compiled C binary, one Node.js server,
SQLite on disk, nginx as the public reverse proxy, and systemd for process
supervision.

## Assumptions to edit if needed

| Setting | Default |
| --- | --- |
| Domain | `hauntedthreads.site`, `www.hauntedthreads.site` |
| VM user | `ubuntu` |
| App directory | `/home/ubuntu/HauntedThreads` |
| Node server | `127.0.0.1:3000` |
| SQLite DB | `data/haunted-threads.db` |
| systemd service | `haunted-threads.service` |

If your VM username or app path differs, update the files in `deploy/` before
copying them into `/etc/systemd/system`.

## 1. Oracle Cloud VM checklist

Recommended VM:

- Ubuntu 24.04 LTS or 22.04 LTS
- Ampere A1 Always Free if available
- 50 GB boot volume
- SSH key authentication enabled

Oracle VCN/security list ingress rules:

- TCP 22 from your IP only
- TCP 80 from `0.0.0.0/0`
- TCP 443 from `0.0.0.0/0`

Ubuntu firewall:

```bash
sudo ufw allow OpenSSH
sudo ufw allow 'Nginx Full'
sudo ufw enable
sudo ufw status
```

## 2. DNS checklist

Create DNS records wherever you bought `hauntedthreads.site`:

```text
A      hauntedthreads.site       <ORACLE_VM_PUBLIC_IPV4>
A      www.hauntedthreads.site   <ORACLE_VM_PUBLIC_IPV4>
```

Optional if your DNS provider supports it:

```text
AAAA   hauntedthreads.site       <ORACLE_VM_IPV6>
AAAA   www.hauntedthreads.site   <ORACLE_VM_IPV6>
```

Check DNS:

```bash
dig +short hauntedthreads.site
dig +short www.hauntedthreads.site
```

## 3. Install server dependencies

```bash
sudo apt update
sudo apt install -y build-essential make git curl nginx sqlite3
curl -fsSL https://deb.nodesource.com/setup_22.x | sudo -E bash -
sudo apt install -y nodejs
node --version
```

The app needs Node.js 22+ for the built-in SQLite module.

## 4. Clone and build

```bash
cd /home/ubuntu
git clone https://github.com/nilaycarleton/Ghost_Hunter_Simulation.git HauntedThreads
cd HauntedThreads
make
mkdir -p data backups
cp deploy/haunted-threads.env.example .env
nano .env
```

## 5. Production `.env` checklist

Required:

```dotenv
NODE_ENV=production
CLERK_PUBLISHABLE_KEY=pk_live_or_test_value
CLERK_SECRET_KEY=sk_live_or_test_value
CLERK_AUTHORIZED_PARTIES=https://hauntedthreads.site,https://www.hauntedthreads.site
PUBLIC_FRAME_ANCESTORS=https://hauntedthreads.site,https://www.hauntedthreads.site
HOST=127.0.0.1
PORT=3000
DATABASE_PATH=data/haunted-threads.db
MAX_CONCURRENT_SESSIONS=4
SESSION_TIMEOUT_MS=60000
AI_PROVIDER=template
```

Do not commit `.env`. It is ignored by Git.

Use `AI_PROVIDER=template` in production unless you intentionally install and
supervise a local Ollama service. The template provider costs nothing and cannot
run out of API credits.

## 6. Clerk production checklist

In the Clerk dashboard:

1. Add `https://hauntedthreads.site` to allowed origins.
2. Add `https://www.hauntedthreads.site` if you plan to use `www`.
3. Use matching publishable/secret keys in `.env`.
4. Restart the app after editing `.env`:

```bash
sudo systemctl restart haunted-threads
```

## 7. Install systemd service

```bash
sudo cp deploy/haunted-threads.service /etc/systemd/system/haunted-threads.service
sudo systemctl daemon-reload
sudo systemctl enable --now haunted-threads
sudo systemctl status haunted-threads
```

Useful logs:

```bash
journalctl -u haunted-threads -n 100 --no-pager
journalctl -u haunted-threads -f
```

Local health check:

```bash
curl http://127.0.0.1:3000/api/health
```

Expected:

```json
{"status":"ok","activeSessions":0,"auth":"enabled"}
```

## 8. Install nginx reverse proxy

```bash
sudo cp deploy/nginx-haunted-threads.conf /etc/nginx/sites-available/haunted-threads
sudo ln -sf /etc/nginx/sites-available/haunted-threads /etc/nginx/sites-enabled/haunted-threads
sudo nginx -t
sudo systemctl reload nginx
```

HTTP check:

```bash
curl -I http://hauntedthreads.site
curl http://hauntedthreads.site/api/health
```

## 9. Enable HTTPS with Certbot

```bash
sudo apt install -y certbot python3-certbot-nginx
sudo certbot --nginx -d hauntedthreads.site -d www.hauntedthreads.site
```

Renewal check:

```bash
sudo certbot renew --dry-run
```

HTTPS check:

```bash
curl -I https://hauntedthreads.site
curl https://hauntedthreads.site/api/health
```

## 10. Enable SQLite backups

Install the backup timer:

```bash
sudo cp deploy/backup-haunted-threads.service /etc/systemd/system/backup-haunted-threads.service
sudo cp deploy/backup-haunted-threads.timer /etc/systemd/system/backup-haunted-threads.timer
sudo systemctl daemon-reload
sudo systemctl enable --now backup-haunted-threads.timer
systemctl list-timers | grep haunted
```

Run one backup immediately:

```bash
sudo systemctl start backup-haunted-threads.service
ls -lh backups/
```

By default, backups are written to `backups/` and compressed as
`haunted-threads-YYYYMMDDTHHMMSSZ.db.gz`. Files older than 14 days are deleted.

For an off-VM backup, periodically copy `backups/*.db.gz` to your computer or
object storage.

## 11. Updating the app

```bash
cd /home/ubuntu/HauntedThreads
git pull
make
sudo systemctl restart haunted-threads
curl https://hauntedthreads.site/api/health
```

If database migrations were added, the Node service applies them on startup.

## Troubleshooting

### `auth` says `guest-only`

Check `.env`:

```bash
grep -E 'CLERK_PUBLISHABLE_KEY|CLERK_AUTHORIZED_PARTIES' .env
sudo systemctl restart haunted-threads
curl http://127.0.0.1:3000/api/health
```

### nginx returns 502

The Node service is probably down or listening on the wrong host/port.

```bash
sudo systemctl status haunted-threads
journalctl -u haunted-threads -n 100 --no-pager
curl http://127.0.0.1:3000/api/health
```

### Certbot cannot issue a certificate

Check DNS and port 80:

```bash
dig +short hauntedthreads.site
sudo ufw status
curl -I http://hauntedthreads.site
```

Oracle security lists must allow TCP 80 and 443.

### Public replay streams hang

Make sure nginx buffering is off:

```nginx
proxy_buffering off;
proxy_read_timeout 75s;
```

Then reload nginx:

```bash
sudo nginx -t
sudo systemctl reload nginx
```

### SQLite backups fail

Check permissions:

```bash
ls -ld data backups
ls -l data/haunted-threads.db
journalctl -u backup-haunted-threads.service -n 50 --no-pager
```

The `ubuntu` user must be able to read `data/haunted-threads.db` and write to
`backups/`.
