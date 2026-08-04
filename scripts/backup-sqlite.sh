#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${ROOT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DATABASE_PATH="${DATABASE_PATH:-$ROOT_DIR/data/haunted-threads.db}"
BACKUP_DIR="${BACKUP_DIR:-$ROOT_DIR/backups}"
KEEP_DAYS="${KEEP_DAYS:-14}"

if [[ ! -f "$DATABASE_PATH" ]]; then
  echo "Database not found: $DATABASE_PATH" >&2
  exit 1
fi

mkdir -p "$BACKUP_DIR"
timestamp="$(date -u +%Y%m%dT%H%M%SZ)"
target="$BACKUP_DIR/haunted-threads-$timestamp.db"

if command -v sqlite3 >/dev/null 2>&1; then
  sqlite3 "$DATABASE_PATH" ".backup '$target'"
else
  cp "$DATABASE_PATH" "$target"
fi

gzip -f "$target"
find "$BACKUP_DIR" -name 'haunted-threads-*.db.gz' -mtime +"$KEEP_DAYS" -delete
echo "Created backup: $target.gz"
