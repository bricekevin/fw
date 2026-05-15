# Deploy - Production Deployment to Mac PROD (Docker)

**Usage:** `/9_deploy`

**Purpose:** Push-driven deploys to a Mac prod server, fully automated via GitHub API + Cloudflare API + SSH. Merge to `main` → auto-deploy in ~7 min (CI ~6 + deploy ~1).

**Two modes, auto-detected:**
1. **First-run setup** — provisions everything: workflows, repo secrets, self-hosted runner, Cloudflare tunnel, DNS, prod Mac bootstrap (zip → scp → docker compose up). Only irreducibly-manual steps (macOS auto-login, Full Disk Access) are surfaced to the user.
2. **Subsequent deploy** — drift-checks runner + tunnel + DNS, pushes the branch, opens/merges a PR, watches the deploy run, verifies version on the public health endpoint.

**Architecture:**
```
dev mac ─push→ branch ─PR→ GitHub Actions CI
                                │
                                ▼ (only on green)
                       merge to main
                                │
                                ▼ (workflow_run trigger)
                       Deploy Prod workflow
                                │
                                ▼ runs on:
                       self-hosted runner @ prod mac
                                │
                                ▼
                       backup → pull → build → up → smoke
                                │
                          ┌─────┴─────┐
                          │           │
                      pass tag    fail rollback + email

Public traffic:
  user → app.<domain> → Cloudflare edge → Cloudflare Tunnel
       → cloudflared (prod mac) → http://localhost:<port> → Docker frontend
                                                              │
                                                              ▼ (reverse proxy)
                                                          Docker backend
```

**No inbound network on prod.** Cloudflare Tunnel is outbound-only — prod Mac can sit behind NAT, no port forwarding, no inbound firewall holes.

---

## REQUIRED ACCESS (PREFLIGHT)

The command bails fast if any of these are missing.

| What | How it's used | How to set up |
|---|---|---|
| `gh` CLI authenticated with `repo`, `workflow`, `admin:repo_hook` scopes | Create secrets, fetch runner registration token, create PRs, merge, watch runs | `gh auth login --scopes repo,workflow,admin:repo_hook` |
| `CLOUDFLARE_API_TOKEN` env var | Create tunnel, create DNS, verify tunnel status | Cloudflare dashboard → My Profile → API Tokens → "Edit zone DNS" + "Cloudflare Tunnel:Edit" scopes |
| SSH to the prod Mac (passwordless, key-based) | Bootstrap runner, install cloudflared, scp zip, start services | `ssh-copy-id <user>@<prod-mac>` once; verify `ssh <prod-mac-alias> echo ok` |
| `jq`, `unzip`, `cloudflared` on dev mac | API JSON parsing, packaging, tunnel CLI | `brew install jq unzip cloudflared` |

---

## STEP 1: DETECT MODE

```bash
if [ -f .github/workflows/deploy-prod.yml ] \
   && [ -f .github/workflows/ci.yml ] \
   && [ -f .starterpack/deploy-state.json ]; then
  MODE="deploy"
else
  MODE="setup"
fi
echo "Mode: $MODE"
```

- `setup` → continue to **STEP 2**
- `deploy` → jump to **STEP 20**

---

# === FIRST-RUN SETUP (STEPS 2-10) ===

## STEP 2: GATHER INFO + PREFLIGHT

Ask the user (or read from `git remote` / existing files) — **in one message**:

```
🚀 PROD DEPLOY SETUP — FIRST RUN

I'll provision everything via GitHub API, Cloudflare API, and SSH. Need:

1. GitHub repo? (owner/repo, e.g., kevinbrice/myapp)
   ⤷ Will autodetect from `git remote get-url origin` if available.

2. Production hostname? (public URL, e.g., app.brice.com)
   ⤷ Must be a subdomain of a Cloudflare-managed zone.

3. Cloudflare zone? (the apex domain, e.g., brice.com)

4. App's internal port on the prod Mac? (the port Docker exposes locally,
   e.g., 3000 for frontend that proxies /api → backend)

5. Health endpoint path? (default: /api/v1/health)

6. Prod Mac SSH alias? (e.g., brice-prod — must exist in ~/.ssh/config
   OR provide user@host directly)

7. PROD_DIR on the prod Mac? (canonical repo path, e.g., /Users/kevinbrice/GIT/myapp)
   ⤷ Will be created via SSH if it doesn't exist.

8. Operator email for failure notifications?

9. Email provider? (default: Resend — needs RESEND_API_KEY)
   ⤷ Paste the key now; will be stored as a GitHub Actions secret via API.

10. Multi-tenant prod Mac? (y/n)
    ⤷ Yes if a shared `gateway-nginx` container already owns :80/:443 on the
      prod Mac and routes by Host header to other apps. In that case this
      app does NOT publish host ports — it joins the `web-gateway` external
      Docker network, exposes its port internally, and a per-app nginx site
      conf gets dropped into the gateway's `conf.d/`. See the
      **MULTI-TENANT VARIANT** section at the bottom for the deltas to
      STEP 3, STEP 5, and STEP 6.
```

After gathering, store in shell vars and run preflight:

```bash
# Required env / tools
gh auth status 2>&1 | grep -q "Logged in"  || { echo "ERR: gh not authenticated. Run: gh auth login --scopes repo,workflow,admin:repo_hook"; exit 1; }
[ -n "$CLOUDFLARE_API_TOKEN" ]              || { echo "ERR: CLOUDFLARE_API_TOKEN env var missing."; exit 1; }
command -v jq >/dev/null                    || { echo "ERR: jq missing. brew install jq"; exit 1; }
command -v cloudflared >/dev/null           || { echo "ERR: cloudflared missing. brew install cloudflared"; exit 1; }

# Verify CF token
CF_TOKEN_OK=$(curl -fsS "https://api.cloudflare.com/client/v4/user/tokens/verify" \
  -H "Authorization: Bearer $CLOUDFLARE_API_TOKEN" | jq -r .success)
[ "$CF_TOKEN_OK" = "true" ] || { echo "ERR: Cloudflare token invalid."; exit 1; }

# Verify gh has the right scopes
gh api user >/dev/null || { echo "ERR: gh API call failed."; exit 1; }

# Verify SSH to prod
ssh -o BatchMode=yes -o ConnectTimeout=5 "$PROD_SSH" echo ok \
  || { echo "ERR: SSH to $PROD_SSH failed. Run: ssh-copy-id $PROD_SSH"; exit 1; }

# Verify the GitHub repo is reachable
gh repo view "$GH_REPO" >/dev/null \
  || { echo "ERR: gh can't see $GH_REPO. Check the slug or auth."; exit 1; }

echo "Preflight: all good."
```

If anything fails, surface the exact remediation command and stop. Do not proceed with a half-broken setup.

---

## STEP 3: SCAFFOLD LOCAL FILES

Create everything that lives in the repo. Same template as before, with hardened gotcha-avoidance baked in. Files created:

- `.github/workflows/ci.yml` — backend tests + frontend build + e2e smoke + audit. Permissions block: `contents: read`, `pull-requests: read` (gitleaks needs it on private repos).
- `.github/workflows/gitleaks.yml` — secret scan with `pull-requests: read`.
- `.github/workflows/deploy-prod.yml` — `runs-on: [self-hosted, macos, prod]`, `concurrency: cancel-in-progress: false`, backup → pull → build → up → smoke → tag → rollback-on-fail → notify.
- `scripts/notify_deploy_failure.sh` — Resend API caller.
- `ops/com.<project>.app.plist` — launchd-on-login `docker compose up -d`.
- `ops/cloudflared.config.yml` — tunnel routing config (templated with hostname + port).
- `docker-compose.prod.yml` — `restart: unless-stopped`, healthcheck on backend, `environment: APP_VERSION: ${APP_VERSION:-dev}`, frontend `build.args: VITE_APP_VERSION`.
- `apps/web/Dockerfile.prod` — `ARG VITE_APP_VERSION="dev"`, `ENV` propagation, `RUN pnpm build`.
- Backend `main.py` (or equivalent) — `APP_VERSION = os.getenv("APP_VERSION", "dev")`, `/api/v1/health` returns `{"status": ..., "version": APP_VERSION}`.
- Frontend `vite-env.d.ts` — `VITE_APP_VERSION?: string` declaration.

**Deploy-prod.yml template (concrete values substituted from STEP 2):**

```yaml
name: Deploy Prod
on:
  workflow_run:
    workflows: ["CI"]
    types: [completed]
    branches: [main]

concurrency:
  group: prod-deploy
  cancel-in-progress: false

jobs:
  deploy:
    if: ${{ github.event.workflow_run.conclusion == 'success' }}
    runs-on: [self-hosted, macos, prod]
    env:
      PROD_DIR: <PROD_DIR>
      HEALTH_URL: https://<PROD_HOSTNAME><HEALTH_PATH>
    steps:
      - name: Record current tag for rollback
        id: prev-tag
        run: |
          cd "$PROD_DIR"
          PREV=$(git tag --list 'prod-*' --sort=-creatordate | head -n 1)
          echo "prev_tag=$PREV" >> "$GITHUB_OUTPUT"
      - name: Backup database
        run: |
          cd "$PROD_DIR"
          [ -x backend/scripts/backup_db.sh ] && bash backend/scripts/backup_db.sh || echo "no backup script — skipping"
      - name: Pull latest main
        run: cd "$PROD_DIR" && git fetch --tags --prune origin && git pull --ff-only origin main
      - name: Compute build version
        run: |
          cd "$PROD_DIR"
          VERSION=$(git describe --tags --match 'v*' --long --always --abbrev=7)
          echo "APP_VERSION=$VERSION" >> "$GITHUB_ENV"
          echo "VITE_APP_VERSION=$VERSION" >> "$GITHUB_ENV"
      - name: Build
        run: cd "$PROD_DIR" && docker compose -f docker-compose.prod.yml build
      - name: Deploy
        run: cd "$PROD_DIR" && docker compose -f docker-compose.prod.yml up -d
      - name: Wait for health
        run: sleep 30
      - name: Smoke test
        run: |
          for i in {1..6}; do
            if curl -fsS "$HEALTH_URL"; then exit 0; fi
            sleep 5
          done
          exit 1
      - name: Tag successful release
        if: success()
        run: |
          cd "$PROD_DIR"
          TAG="prod-$(date +%Y%m%d-%H%M)"
          git tag "$TAG" && git push origin "$TAG"
      - name: Rollback on smoke failure
        if: failure() && steps.prev-tag.outputs.prev_tag != ''
        run: |
          cd "$PROD_DIR"
          git checkout "${{ steps.prev-tag.outputs.prev_tag }}" -- .
          docker compose -f docker-compose.prod.yml up -d --build
          sleep 30 && curl -fsS "$HEALTH_URL"
      - name: Notify operator on failure
        if: failure()
        env:
          RESEND_API_KEY: ${{ secrets.RESEND_API_KEY }}
          OPERATOR_EMAIL: ${{ secrets.OPERATOR_EMAIL }}
        run: bash "$PROD_DIR/scripts/notify_deploy_failure.sh" "$GITHUB_SHA" "$GITHUB_RUN_ID"
```

**Cloudflared config template (`ops/cloudflared.config.yml`) — gets written with real values, then scp'd to prod in STEP 6:**

```yaml
tunnel: <TUNNEL_UUID>
credentials-file: <HOME>/.cloudflared/<TUNNEL_UUID>.json

ingress:
  - hostname: <PROD_HOSTNAME>
    service: http://localhost:<APP_INTERNAL_PORT>
  - service: http_status:404
```

Commit the scaffold to a feature branch — but **don't merge yet**, the runner doesn't exist:

```bash
git checkout -b feat/prod-deploy-pipeline
git add .github/ scripts/ ops/ docker-compose.prod.yml apps/web/Dockerfile.prod \
        apps/web/src/vite-env.d.ts backend/app/main.py
git commit -m "feat(deploy): scaffold prod deploy pipeline + cloudflared config"
git push -u origin feat/prod-deploy-pipeline
```

---

## STEP 4: GITHUB API — SECRETS + RUNNER TOKEN

All via `gh` — no manual UI clicks.

```bash
# Set secrets (idempotent — gh upserts)
gh secret set RESEND_API_KEY  --repo "$GH_REPO" --body "$RESEND_KEY"
gh secret set OPERATOR_EMAIL  --repo "$GH_REPO" --body "$OPERATOR_EMAIL"

# Fetch a runner registration token (valid 1 hour, single-use)
RUNNER_TOKEN=$(gh api -X POST \
  "/repos/$GH_REPO/actions/runners/registration-token" \
  --jq .token)
[ -n "$RUNNER_TOKEN" ] || { echo "ERR: couldn't get runner token"; exit 1; }

# Pin the runner version (latest stable known to work; bump periodically)
RUNNER_VERSION="2.334.0"
RUNNER_SHA=$(curl -fsS \
  "https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/actions-runner-osx-arm64-${RUNNER_VERSION}.tar.gz.sha256" \
  | awk '{print $1}')
# Fallback: hardcode the known SHA if the .sha256 URL ever changes
```

**Idempotency note:** if a runner with the chosen name already exists for this repo, the second registration will replace it. To avoid surprise, list first:

```bash
gh api "/repos/$GH_REPO/actions/runners" --jq '.runners[] | {id, name, status, labels: [.labels[].name]}'
```

If a runner with the same name is online, the user probably already ran setup — abort with a clear message and recommend `MODE=deploy`.

---

## STEP 5: CLOUDFLARE API — TUNNEL + DNS

Reusable helper (define once near the top of execution):

```bash
cf() { curl -fsS -H "Authorization: Bearer $CLOUDFLARE_API_TOKEN" -H "Content-Type: application/json" "$@"; }
```

```bash
# 5.1 — Resolve account ID and zone ID
ACCOUNT_ID=$(cf "https://api.cloudflare.com/client/v4/accounts" | jq -r '.result[0].id')
ZONE_ID=$(cf "https://api.cloudflare.com/client/v4/zones?name=$CF_ZONE" | jq -r '.result[0].id')
[ "$ZONE_ID" = "null" ] && { echo "ERR: Cloudflare zone $CF_ZONE not found in your account."; exit 1; }

# 5.2 — Tunnel name (deterministic so re-runs are safe)
TUNNEL_NAME="${GH_REPO//\//-}-prod"  # e.g., kevinbrice-myapp-prod

# 5.3 — Check if tunnel already exists
EXISTING_TUNNEL=$(cf "https://api.cloudflare.com/client/v4/accounts/$ACCOUNT_ID/cfd_tunnel?name=$TUNNEL_NAME&is_deleted=false" \
  | jq -r '.result[0].id // empty')

if [ -n "$EXISTING_TUNNEL" ]; then
  echo "Tunnel '$TUNNEL_NAME' already exists ($EXISTING_TUNNEL) — reusing."
  TUNNEL_ID="$EXISTING_TUNNEL"
  # We can't recover the secret; need to regenerate credentials JSON.
  # Simplest path: delete + recreate. Safer path: rotate via API:
  TUNNEL_SECRET=$(openssl rand -base64 32)
  cf -X PATCH "https://api.cloudflare.com/client/v4/accounts/$ACCOUNT_ID/cfd_tunnel/$TUNNEL_ID" \
    -d "{\"tunnel_secret\": \"$(echo -n "$TUNNEL_SECRET" | base64)\"}" >/dev/null
else
  # 5.4 — Create new tunnel
  TUNNEL_SECRET=$(openssl rand -base64 32)
  CREATE_RESP=$(cf -X POST "https://api.cloudflare.com/client/v4/accounts/$ACCOUNT_ID/cfd_tunnel" \
    -d "{
      \"name\": \"$TUNNEL_NAME\",
      \"tunnel_secret\": \"$(echo -n "$TUNNEL_SECRET" | base64)\",
      \"config_src\": \"local\"
    }")
  TUNNEL_ID=$(echo "$CREATE_RESP" | jq -r '.result.id')
  [ "$TUNNEL_ID" = "null" ] && { echo "ERR: tunnel create failed: $CREATE_RESP"; exit 1; }
  echo "Tunnel created: $TUNNEL_ID"
fi

# 5.5 — Write credentials JSON for transfer to prod
mkdir -p .starterpack/cloudflared
CREDS_FILE=".starterpack/cloudflared/$TUNNEL_ID.json"
cat > "$CREDS_FILE" <<EOF
{
  "AccountTag": "$ACCOUNT_ID",
  "TunnelID": "$TUNNEL_ID",
  "TunnelName": "$TUNNEL_NAME",
  "TunnelSecret": "$(echo -n "$TUNNEL_SECRET" | base64)"
}
EOF
chmod 600 "$CREDS_FILE"

# 5.6 — Write the actual cloudflared config for the prod Mac
cat > ops/cloudflared.config.yml <<EOF
tunnel: $TUNNEL_ID
credentials-file: /Users/<PROD_USER>/.cloudflared/$TUNNEL_ID.json

ingress:
  - hostname: $PROD_HOSTNAME
    service: http://localhost:$APP_INTERNAL_PORT
  - service: http_status:404
EOF
# (PROD_USER is templated in via sed before scp in STEP 6)

# 5.7 — DNS: CNAME <PROD_HOSTNAME> → <TUNNEL_ID>.cfargotunnel.com
TUNNEL_CNAME="${TUNNEL_ID}.cfargotunnel.com"
EXISTING_DNS=$(cf "https://api.cloudflare.com/client/v4/zones/$ZONE_ID/dns_records?name=$PROD_HOSTNAME&type=CNAME" \
  | jq -r '.result[0].id // empty')

if [ -n "$EXISTING_DNS" ]; then
  cf -X PUT "https://api.cloudflare.com/client/v4/zones/$ZONE_ID/dns_records/$EXISTING_DNS" \
    -d "{\"type\":\"CNAME\",\"name\":\"$PROD_HOSTNAME\",\"content\":\"$TUNNEL_CNAME\",\"proxied\":true}" >/dev/null
  echo "DNS updated: $PROD_HOSTNAME → $TUNNEL_CNAME (proxied)"
else
  cf -X POST "https://api.cloudflare.com/client/v4/zones/$ZONE_ID/dns_records" \
    -d "{\"type\":\"CNAME\",\"name\":\"$PROD_HOSTNAME\",\"content\":\"$TUNNEL_CNAME\",\"proxied\":true}" >/dev/null
  echo "DNS created: $PROD_HOSTNAME → $TUNNEL_CNAME (proxied)"
fi
```

**Important:** The credentials JSON contains the tunnel secret. It's written to `.starterpack/cloudflared/` locally (gitignored — see STEP 10) for the duration of setup. After scp'ing to prod, you can delete the local copy.

Add to `.gitignore`:
```
.starterpack/cloudflared/
```

---

## STEP 6: PROD MAC BOOTSTRAP (SSH)

Everything from here runs over SSH against `$PROD_SSH`. Make it idempotent — re-runs should be safe.

### 6.1 — Prerequisites on prod Mac

```bash
ssh "$PROD_SSH" bash <<'REMOTE'
set -euo pipefail

# Homebrew (Apple Silicon path)
if ! command -v brew >/dev/null; then
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
  eval "$(/opt/homebrew/bin/brew shellenv)"
fi

# Docker Desktop
if ! command -v docker >/dev/null; then
  brew install --cask docker
  open -a Docker
  # Wait for Docker daemon to start
  for i in {1..30}; do
    docker ps >/dev/null 2>&1 && break
    sleep 2
  done
fi
docker ps >/dev/null || { echo "ERR: Docker not running"; exit 1; }

# cloudflared
command -v cloudflared >/dev/null || brew install cloudflared

# jq (for the launchd-on-login script, optional but useful)
command -v jq >/dev/null || brew install jq
REMOTE
```

### 6.2 — Zip project locally + SCP to prod

```bash
# Create a clean zip of tracked files only — no node_modules, no .env
PROJECT_NAME=$(basename "$(git rev-parse --show-toplevel)")
SHORT_SHA=$(git rev-parse --short HEAD)
ZIP="/tmp/${PROJECT_NAME}-bootstrap-${SHORT_SHA}.zip"

git archive --format=zip --output="$ZIP" HEAD
echo "Built zip: $ZIP ($(du -h "$ZIP" | cut -f1))"

# Transfer
scp "$ZIP" "$PROD_SSH:/tmp/"

# Unpack on prod
ssh "$PROD_SSH" bash <<REMOTE
set -euo pipefail
mkdir -p "$PROD_DIR"
cd "$PROD_DIR"
unzip -o "/tmp/$(basename "$ZIP")"
rm "/tmp/$(basename "$ZIP")"

# This bootstrap dir needs to be a git checkout so the deploy-prod.yml's
# 'git pull' step works on every subsequent deploy. Initialize it:
if [ ! -d .git ]; then
  git init
  git remote add origin "https://github.com/$GH_REPO.git"
  git fetch origin main
  # Reset to match origin/main so working tree == HEAD
  git reset --hard origin/main || true
fi
REMOTE
```

**Why zip then `git init`:** the zip carries the working tree without `.git`. After unzip we initialize a fresh git checkout pointing at the GitHub remote so the deploy workflow's `git fetch && git pull` step has a remote to talk to.

**Alternative if the repo is public:** skip the zip and just `git clone` on the prod Mac. The zip path is the resilient choice (works for private repos without setting up auth on the prod Mac).

### 6.3 — Install actions-runner on prod Mac

```bash
ssh "$PROD_SSH" bash <<REMOTE
set -euo pipefail

# Skip if already installed
if launchctl list | grep -q actions.runner; then
  echo "Runner already installed — skipping install."
  exit 0
fi

cd ~
mkdir -p actions-runner && cd actions-runner

curl -fsSL -o "actions-runner-osx-arm64-$RUNNER_VERSION.tar.gz" \
  "https://github.com/actions/runner/releases/download/v$RUNNER_VERSION/actions-runner-osx-arm64-$RUNNER_VERSION.tar.gz"

# Verify hash
echo "$RUNNER_SHA  actions-runner-osx-arm64-$RUNNER_VERSION.tar.gz" | shasum -a 256 -c

tar xzf "actions-runner-osx-arm64-$RUNNER_VERSION.tar.gz"

# Unattended config — exact labels match deploy-prod.yml runs-on
./config.sh \
  --url "https://github.com/$GH_REPO" \
  --token "$RUNNER_TOKEN" \
  --labels self-hosted,macos,prod \
  --name "${PROJECT_NAME}-prod" \
  --work _work \
  --unattended

./svc.sh install
./svc.sh start

# Verify
sleep 3
launchctl list | grep actions.runner || { echo "ERR: runner service didn't start"; exit 1; }
REMOTE
```

### 6.4 — Install + start cloudflared on prod Mac

```bash
# Push the credentials JSON
PROD_USER=$(ssh "$PROD_SSH" whoami)
ssh "$PROD_SSH" "mkdir -p /Users/$PROD_USER/.cloudflared && chmod 700 /Users/$PROD_USER/.cloudflared"
scp ".starterpack/cloudflared/$TUNNEL_ID.json" "$PROD_SSH:/Users/$PROD_USER/.cloudflared/"

# Templated config (substitute the real PROD_USER path)
sed "s|/Users/<PROD_USER>/|/Users/$PROD_USER/|g" ops/cloudflared.config.yml \
  | ssh "$PROD_SSH" "cat > /Users/$PROD_USER/.cloudflared/config.yml"

# Install as launchd service (system-wide, runs as the user)
ssh "$PROD_SSH" TUNNEL_ID="$TUNNEL_ID" bash <<'REMOTE'
set -euo pipefail

# Modern cloudflared installs at /Library/LaunchDaemons/com.cloudflare.cloudflared.plist
sudo cloudflared service install 2>&1 || true

# Verify the service registered
sleep 3
sudo launchctl list 2>/dev/null | grep com.cloudflare.cloudflared \
  || launchctl list | grep com.cloudflare.cloudflared \
  || { echo "ERR: cloudflared service didn't register"; exit 1; }

# Health: ask the tunnel to report itself
cloudflared tunnel info "$TUNNEL_ID" 2>/dev/null | head -20 || true
REMOTE

# Verify tunnel is connected to Cloudflare's edge (poll the CF API — dig won't help for proxied records)
echo "Waiting for tunnel to register with Cloudflare edge (max 60s)..."
for i in {1..12}; do
  TUNNEL_STATUS=$(cf "https://api.cloudflare.com/client/v4/accounts/$ACCOUNT_ID/cfd_tunnel/$TUNNEL_ID" | jq -r '.result.status')
  if [ "$TUNNEL_STATUS" = "healthy" ]; then
    echo "Tunnel connected: $TUNNEL_STATUS"
    break
  fi
  sleep 5
done
[ "$TUNNEL_STATUS" = "healthy" ] || echo "WARN: tunnel status is '$TUNNEL_STATUS' — may catch up in a minute"

# Local copy of credentials no longer needed — delete
rm -f ".starterpack/cloudflared/$TUNNEL_ID.json"
echo "Local credentials deleted (still on prod at ~/.cloudflared/)."
```

### 6.5 — Docker compose first-run on prod

```bash
ssh "$PROD_SSH" PROD_DIR="$PROD_DIR" APP_INTERNAL_PORT="$APP_INTERNAL_PORT" HEALTH_PATH="$HEALTH_PATH" bash <<'REMOTE'
set -euo pipefail
cd "$PROD_DIR"

# Copy any .env.example to .env if .env doesn't exist
if [ -f .env.example ] && [ ! -f .env ]; then
  cp .env.example .env
  echo "WARN: Created .env from .env.example — review and fill in real values."
fi

# Build + up
docker compose -f docker-compose.prod.yml build
docker compose -f docker-compose.prod.yml up -d

# Wait for healthcheck
for i in {1..12}; do
  HEALTHY=$(docker compose -f docker-compose.prod.yml ps --format json \
    | jq -r 'select(.Health == "healthy") | .Service' | wc -l)
  TOTAL=$(docker compose -f docker-compose.prod.yml ps --format json | wc -l)
  if [ "$HEALTHY" -gt 0 ] && [ "$HEALTHY" -eq "$TOTAL" ]; then
    echo "All services healthy."
    break
  fi
  sleep 5
done

# Local sanity check (from prod Mac itself, hits localhost not the tunnel)
curl -fsS "http://localhost:$APP_INTERNAL_PORT$HEALTH_PATH" || \
  echo "WARN: localhost health check failed — check 'docker compose logs'"
REMOTE

# Public sanity check (from dev mac, via tunnel)
echo "Hitting public health endpoint..."
sleep 5
curl -fsS "https://$PROD_HOSTNAME$HEALTH_PATH" \
  && echo "✓ Public endpoint live" \
  || echo "WARN: public endpoint not responding yet — give CF + tunnel another minute"
```

### 6.6 — Install launchd-on-login plist

```bash
# Render the plist with real PROD_DIR + PROJECT_NAME, scp, load
sed "s|<PROD_DIR>|$PROD_DIR|g; s|<project>|$PROJECT_NAME|g" \
  ops/com.<project>.app.plist \
  > "/tmp/com.${PROJECT_NAME}.app.plist"

scp "/tmp/com.${PROJECT_NAME}.app.plist" "$PROD_SSH:/Users/$PROD_USER/Library/LaunchAgents/"
rm "/tmp/com.${PROJECT_NAME}.app.plist"

ssh "$PROD_SSH" "launchctl load -w /Users/$PROD_USER/Library/LaunchAgents/com.${PROJECT_NAME}.app.plist"
```

---

## STEP 7: IRREDUCIBLY-MANUAL UI STEPS

A small set of macOS settings genuinely require GUI interaction. Surface them now:

```
MANUAL STEPS ON PROD MAC (~5 min, requires physical/VNC access):

1. Auto-login (so the runner + cloudflared survive a reboot without manual login):
   System Settings → Users & Groups → "Automatically log in as..." → <user>

   If FileVault is on, auto-login is blocked. Workarounds:
     a) Disable FileVault (only acceptable if the Mac is physically secure)
     b) Reinstall the runner as a LaunchDaemon: 'sudo ./svc.sh install' (in ~/actions-runner)
        cloudflared is already a system service via `sudo cloudflared service install`

2. Verify Docker Desktop is set to start at login:
   Docker Desktop → Settings → General → "Start Docker Desktop when you sign in"
   (already true if installed via 'brew install --cask docker' on most systems; verify)

3. (Conditional) Full Disk Access for /bin/bash — only if any launchd cron job
   touches ~/Documents/. Skip otherwise.
   System Settings → Privacy & Security → Full Disk Access → "+" /bin/bash → ON

Reply 'done' when complete (or 'skip' if not applicable).
```

---

## STEP 8: BASELINE + V0.1.0 TAGS

```bash
# Baseline tag on prod (so the deploy workflow's rollback step has a target)
ssh "$PROD_SSH" bash <<REMOTE
cd "$PROD_DIR"
git fetch origin
TAG="prod-baseline-\$(date +%Y%m%d-%H%M)"
git tag "\$TAG"
git push origin "\$TAG" 2>/dev/null || true   # may fail until PR is merged + remote allows
REMOTE

# Local v0.1.0 (gives the health endpoint a real version string)
git checkout main && git pull --ff-only
if ! git rev-parse v0.1.0 >/dev/null 2>&1; then
  git tag v0.1.0
  git push origin v0.1.0
fi
```

---

## STEP 9: MERGE PR + WATCH FIRST DEPLOY

```bash
# Merge the feat/prod-deploy-pipeline PR
PR_NUM=$(gh pr view feat/prod-deploy-pipeline --json number -q .number)
gh pr merge "$PR_NUM" --squash --delete-branch

# Watch CI + Deploy Prod
gh run watch
```

Verify:

```bash
sleep 10
PROD_VERSION=$(curl -fsS "https://$PROD_HOSTNAME$HEALTH_PATH" | jq -r .version)
EXPECTED=$(git describe --tags --match 'v*' --long --always --abbrev=7)
echo "Prod:     $PROD_VERSION"
echo "Expected: $EXPECTED"
[ "$PROD_VERSION" = "$EXPECTED" ] && echo "✓ Verified" || echo "✗ Mismatch — check Dockerfile/compose env wiring"
```

---

## STEP 10: WRITE STATE FILE + SUMMARY

```bash
mkdir -p .starterpack
cat > .starterpack/deploy-state.json <<EOF
{
  "mode": "deployed",
  "first_deploy_date": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "github_repo": "$GH_REPO",
  "prod_hostname": "$PROD_HOSTNAME",
  "health_url": "https://$PROD_HOSTNAME$HEALTH_PATH",
  "prod_ssh": "$PROD_SSH",
  "prod_dir": "$PROD_DIR",
  "prod_user": "$PROD_USER",
  "app_internal_port": $APP_INTERNAL_PORT,
  "operator_email": "$OPERATOR_EMAIL",
  "cloudflare": {
    "zone": "$CF_ZONE",
    "zone_id": "$ZONE_ID",
    "account_id": "$ACCOUNT_ID",
    "tunnel_id": "$TUNNEL_ID",
    "tunnel_name": "$TUNNEL_NAME"
  },
  "runner": {
    "version": "$RUNNER_VERSION",
    "name": "${PROJECT_NAME}-prod",
    "labels": ["self-hosted", "macos", "prod"]
  }
}
EOF

# Ensure .gitignore protects credentials
grep -q '^\.starterpack/cloudflared/' .gitignore 2>/dev/null \
  || echo '.starterpack/cloudflared/' >> .gitignore

git add .starterpack/deploy-state.json .gitignore
git commit -m "chore(deploy): record first-deploy state"
git push
```

Final summary:

```
✓ FIRST-DEPLOY COMPLETE

GitHub:       $GH_REPO
Public URL:   https://$PROD_HOSTNAME$HEALTH_PATH
Version:      $PROD_VERSION
Runner:       Online ($RUNNER_NAME) at $PROD_DIR
Tunnel:       $TUNNEL_NAME ($TUNNEL_ID)
DNS:          $PROD_HOSTNAME → $TUNNEL_ID.cfargotunnel.com (proxied)
Operator:     $OPERATOR_EMAIL (notifications via Resend)

Subsequent /9_deploy runs will use "deploy" mode:
  drift check → push → PR → merge → watch deploy → verify version.

Quarterly drills:
  1. Power-cycle prod Mac — site should be back within ~3 min.
  2. Break a smoke target → verify auto-rollback + email.
  3. curl /health | jq -r .version should match `git describe` on local main.
```

---

# === SUBSEQUENT DEPLOY (STEPS 20-27) ===

## STEP 20: LOAD STATE

```bash
S=.starterpack/deploy-state.json
GH_REPO=$(jq -r .github_repo "$S")
PROD_HOSTNAME=$(jq -r .prod_hostname "$S")
HEALTH_URL=$(jq -r .health_url "$S")
PROD_SSH=$(jq -r .prod_ssh "$S")
TUNNEL_ID=$(jq -r .cloudflare.tunnel_id "$S")
ZONE_ID=$(jq -r .cloudflare.zone_id "$S")
ACCOUNT_ID=$(jq -r .cloudflare.account_id "$S")
RUNNER_NAME=$(jq -r .runner.name "$S")

CURRENT_BRANCH=$(git branch --show-current)
DEFAULT_BRANCH=$(git symbolic-ref refs/remotes/origin/HEAD 2>/dev/null | sed 's@^refs/remotes/origin/@@' || echo main)
echo "Branch: $CURRENT_BRANCH (default: $DEFAULT_BRANCH)"
```

---

## STEP 21: DRIFT CHECK (the "update command has API automation" piece)

Before pushing anything, confirm prod infrastructure is still healthy. Fixes drift via API where possible; surfaces it otherwise.

```bash
cf() { curl -fsS -H "Authorization: Bearer $CLOUDFLARE_API_TOKEN" -H "Content-Type: application/json" "$@"; }

DRIFT=0

# 21.1 — Runner online?
RUNNER_STATUS=$(gh api "/repos/$GH_REPO/actions/runners" \
  --jq ".runners[] | select(.name == \"$RUNNER_NAME\") | .status")
if [ "$RUNNER_STATUS" != "online" ]; then
  echo "⚠ Runner $RUNNER_NAME is $RUNNER_STATUS (expected: online)"
  echo "  Attempting to restart via SSH..."
  ssh "$PROD_SSH" "cd ~/actions-runner && ./svc.sh stop && ./svc.sh start" \
    && echo "  Runner restarted." \
    || { echo "  Restart failed — investigate manually."; DRIFT=1; }
else
  echo "✓ Runner online"
fi

# 21.2 — Cloudflare tunnel connected?
TUNNEL_STATUS=$(cf "https://api.cloudflare.com/client/v4/accounts/$ACCOUNT_ID/cfd_tunnel/$TUNNEL_ID" \
  | jq -r '.result.status')
# status values: inactive, degraded, healthy, down
if [ "$TUNNEL_STATUS" != "healthy" ]; then
  echo "⚠ Tunnel status: $TUNNEL_STATUS (expected: healthy)"
  echo "  Attempting to restart cloudflared on prod..."
  ssh "$PROD_SSH" "sudo launchctl kickstart -k system/com.cloudflare.cloudflared 2>/dev/null \
    || launchctl kickstart -k gui/\$(id -u)/com.cloudflare.cloudflared 2>/dev/null \
    || true"
  sleep 5
  TUNNEL_STATUS=$(cf "https://api.cloudflare.com/client/v4/accounts/$ACCOUNT_ID/cfd_tunnel/$TUNNEL_ID" \
    | jq -r '.result.status')
  [ "$TUNNEL_STATUS" = "healthy" ] && echo "  Tunnel healthy after restart." || { echo "  Still $TUNNEL_STATUS — investigate."; DRIFT=1; }
else
  echo "✓ Tunnel healthy"
fi

# 21.3 — DNS record points to the right tunnel?
# Note: dig won't help here — Cloudflare-proxied CNAMEs are flattened to edge IPs at the wire.
# We check the record content via the CF API instead.
DNS_CONTENT=$(cf "https://api.cloudflare.com/client/v4/zones/$ZONE_ID/dns_records?name=$PROD_HOSTNAME&type=CNAME" \
  | jq -r '.result[0].content // empty')
EXPECTED_CNAME="${TUNNEL_ID}.cfargotunnel.com"
if [ "$DNS_CONTENT" != "$EXPECTED_CNAME" ]; then
  echo "⚠ DNS CNAME for $PROD_HOSTNAME = '$DNS_CONTENT', expected '$EXPECTED_CNAME'"
  echo "  Repairing via Cloudflare API..."
  EXISTING_DNS=$(cf "https://api.cloudflare.com/client/v4/zones/$ZONE_ID/dns_records?name=$PROD_HOSTNAME&type=CNAME" \
    | jq -r '.result[0].id // empty')
  if [ -n "$EXISTING_DNS" ]; then
    cf -X PUT "https://api.cloudflare.com/client/v4/zones/$ZONE_ID/dns_records/$EXISTING_DNS" \
      -d "{\"type\":\"CNAME\",\"name\":\"$PROD_HOSTNAME\",\"content\":\"${TUNNEL_ID}.cfargotunnel.com\",\"proxied\":true}" >/dev/null \
      && echo "  DNS updated." || DRIFT=1
  else
    cf -X POST "https://api.cloudflare.com/client/v4/zones/$ZONE_ID/dns_records" \
      -d "{\"type\":\"CNAME\",\"name\":\"$PROD_HOSTNAME\",\"content\":\"${TUNNEL_ID}.cfargotunnel.com\",\"proxied\":true}" >/dev/null \
      && echo "  DNS created." || DRIFT=1
  fi
else
  echo "✓ DNS correct"
fi

# 21.4 — Public health endpoint reachable?
if curl -fsS --max-time 10 "$HEALTH_URL" >/dev/null; then
  echo "✓ Health endpoint reachable"
else
  echo "⚠ $HEALTH_URL not responding — current state may be broken before this deploy"
  DRIFT=1
fi

# 21.5 — Secrets still present?
HAS_RESEND=$(gh secret list --repo "$GH_REPO" --json name -q '.[] | select(.name == "RESEND_API_KEY") | .name')
HAS_EMAIL=$(gh secret list --repo "$GH_REPO" --json name -q '.[] | select(.name == "OPERATOR_EMAIL") | .name')
[ -z "$HAS_RESEND" ] && { echo "⚠ RESEND_API_KEY secret missing"; DRIFT=1; }
[ -z "$HAS_EMAIL" ]  && { echo "⚠ OPERATOR_EMAIL secret missing"; DRIFT=1; }

if [ "$DRIFT" -eq 1 ]; then
  echo ""
  echo "Drift detected and not fully auto-fixed. Continue with deploy? (y/n)"
  # Wait for user — don't blindly proceed onto possibly-broken infra
fi
```

---

## STEP 22: COMMIT, PUSH, OPEN/CHECK PR

```bash
# Working tree clean?
if [ -n "$(git status --porcelain)" ]; then
  echo "Uncommitted changes:"
  git status --short
  echo "Commit before proceeding? (y/n)"
  # If yes, prompt for message and commit
fi

# Refuse to deploy from main directly
[ "$CURRENT_BRANCH" = "$DEFAULT_BRANCH" ] && {
  echo "ERR: On $DEFAULT_BRANCH. Create a feature branch first."
  exit 1
}

# Push current branch
git push -u origin "$CURRENT_BRANCH"

# Find or open PR
PR_NUM=$(gh pr view "$CURRENT_BRANCH" --json number -q .number 2>/dev/null || echo "")
if [ -z "$PR_NUM" ]; then
  TITLE=$(git log -1 --pretty=%s)
  gh pr create --base "$DEFAULT_BRANCH" --head "$CURRENT_BRANCH" \
    --title "$TITLE" \
    --body "$(git log "origin/$DEFAULT_BRANCH..$CURRENT_BRANCH" --pretty='- %s' --reverse)"
  PR_NUM=$(gh pr view "$CURRENT_BRANCH" --json number -q .number)
fi
echo "PR: https://github.com/$GH_REPO/pull/$PR_NUM"
```

---

## STEP 23: WAIT FOR CI GREEN

```bash
gh pr checks "$PR_NUM" --watch --interval 30
```

On failure: fetch logs, surface the failing job, **do not auto-fix CI failures** — let the user decide.

---

## STEP 24: MERGE TO MAIN + WATCH DEPLOY

```bash
gh pr merge "$PR_NUM" --squash --delete-branch
git checkout "$DEFAULT_BRANCH"
git pull --ff-only origin "$DEFAULT_BRANCH"

# The merge fires CI → workflow_run → Deploy Prod
echo "Waiting for Deploy Prod to be triggered..."
sleep 30   # give CI a head start

gh run watch
```

---

## STEP 25: POST-DEPLOY VERIFY

```bash
sleep 10

# Version match
PROD_VERSION=$(curl -fsS "$HEALTH_URL" | jq -r .version)
EXPECTED=$(git describe --tags --match 'v*' --long --always --abbrev=7)
echo "Prod:     $PROD_VERSION"
echo "Expected: $EXPECTED"

# Tunnel still healthy after deploy
TUNNEL_STATUS=$(cf "https://api.cloudflare.com/client/v4/accounts/$ACCOUNT_ID/cfd_tunnel/$TUNNEL_ID" | jq -r '.result.status')
echo "Tunnel:   $TUNNEL_STATUS"

# Smoke a non-health route (the deploy workflow already smoke-tested /health)
curl -fsS --max-time 10 "https://$PROD_HOSTNAME/" >/dev/null \
  && echo "✓ Root route responsive" \
  || echo "⚠ Root route not responding"

[ "$PROD_VERSION" = "$EXPECTED" ] && [ "$TUNNEL_STATUS" = "healthy" ] \
  && echo "✓ Deploy verified" \
  || echo "✗ Investigate before declaring success"
```

---

## STEP 26: BUMP RELEASE TAG (OPTIONAL)

```bash
LAST_TAG=$(git tag --list 'v*' --sort=-v:refname | head -n 1)
echo "Last release tag: $LAST_TAG"
echo "Bump major/minor/patch? (or skip)"

# After user picks NEW_TAG:
# git tag $NEW_TAG && git push origin $NEW_TAG
# git commit --allow-empty -m "chore: release $NEW_TAG" && git push origin main
```

---

## STEP 27: SUMMARY

```
✓ DEPLOY COMPLETE

PR:           #$PR_NUM (merged + branch deleted)
Version:      $PROD_VERSION
Public URL:   $HEALTH_URL
Tunnel:       $TUNNEL_STATUS
CI run:       <CI_URL>
Deploy run:   <DEPLOY_URL>
Total time:   <ELAPSED> min from merge
Drift check:  <PASS/FIXED/MANUAL ACTION NEEDED>
```

---

## GOTCHA REFERENCE (DO NOT REINTRODUCE)

These cost 10-30 min each on first attempt. The setup steps above prevent them — listed here so future edits don't strip the safeguards:

1. **`pnpm audit --filter`** doesn't exist — run at workspace root.
2. **`npm ci` against pnpm workspace** fails on `workspace:*` deps.
3. **e2e job missing `pnpm install`** — Playwright needs workspace deps installed first.
4. **`VITE_*` not threaded through compose build args** — must wire Dockerfile ARG + compose `args:` + host env.
5. **Backend env not in compose `environment:` block** — vars in root `.env` are NOT auto-propagated to containers.
6. **gitleaks 403 on private repos** — needs `permissions: pull-requests: read`.
7. **Wrong `PROD_DIR`** — gathered in STEP 2 and now auto-created via SSH so this can't drift.
8. **Runner label mismatch** — registered with `--labels self-hosted,macos,prod --unattended` in STEP 6.3; matches deploy-prod.yml exactly.
9. **launchd PATH empty** — plist uses `bash -lc` to load login PATH.
10. **macOS TCC denies bash** — only relevant if launchd jobs touch `~/Documents/`; STEP 7 surfaces FDA as conditional.
11. **Cloudflare tunnel credentials leaked to git** — `.starterpack/cloudflared/` is auto-added to `.gitignore` in STEP 10 and the local copy is deleted after transfer in STEP 6.4.
12. **Runner registration token expired** — fetched fresh in STEP 4 (valid 1 hour); STEP 4 → STEP 6 happens in the same run.
13. **Bcrypt `$` chars in `.env` get eaten by Compose** — Compose interpolates `env_file:` values, so `$2b$10$...` becomes garbled and bcrypt verify fails ("Illegal arguments to bcrypt"). Fix: double every literal `$` in the .env (`$$2b$$10$$...`). Verify with `docker exec <web> printenv OPERATOR_PASSWORD_HASH` — must show single `$`.
14. **Compose overlay `volumes: []` silently merges with base** — dev bind mounts leak into prod. Use the explicit reset directive: `volumes: !reset []` (and `ports: !reset []`).
15. **Empty `public/` dir breaks Next.js Dockerfile** — `COPY public ./public` fails if the dir doesn't exist. Create `public/.gitkeep` even when the app has no static assets.
16. **Self-hosted runner can't `git pull` private repos** — launchd-spawned runner has no TTY and no keychain, so cached HTTPS credentials don't apply. The deploy-prod.yml workflow already exposes `${{ github.token }}`; use it via URL-rewrite (NOT a `Bearer` header — GitHub's git endpoint wants Basic with `x-access-token` as username):
    ```yaml
    - name: Pull latest main
      run: |
        cd "$PROD_DIR"
        git -c "url.https://x-access-token:${{ github.token }}@github.com/.insteadOf=https://github.com/" \
            fetch --tags --prune origin
        git pull --ff-only origin main
    ```
17. **Cloudflare Tunnel sets `X-Forwarded-Proto: https` but proxies plain HTTP** — if any reverse proxy is between the tunnel and the app (multi-tenant case), it must forward that header verbatim or OAuth/Auth.js redirect_uri lands on `http://...` and bounces. Add `proxy_set_header X-Forwarded-Proto $http_x_forwarded_proto;` in the nginx site conf.
18. **`gh push` rejects workflow file changes** with "refusing to allow an OAuth App to create or update workflow" — the default `gh auth login` scope set lacks `workflow`. Preflight in STEP 2 requests it; if a user re-auths later, surface `gh auth refresh -h github.com -s workflow`.

---

## STOP-AND-ASK CHECKPOINTS

These are spots where the command should pause and surface to the operator rather than auto-decide. Listed so an LLM driving this script doesn't barrel through:

- **`.env` not yet on prod** in STEP 6.5 — wait for the operator to drop it in. Don't auto-populate from `.env.example` beyond the comment-only template.
- **Port 80 already bound on prod Mac** by something unfamiliar (not `gateway-nginx`, not a known previous app) — ask before continuing.
- **DB restore prints any `ERROR:` line** during `psql` import — stop, show the verbatim error, do not proceed to "up".
- **Tunnel status != `healthy` after 60s** in STEP 6.4 — surface, don't loop forever.
- **Auto-deploy CI green but smoke fails AND rollback also fails** — leave the site down, do not automatically retry. Site was likely broken before this deploy.
- **Any secret in the source `.env` looks malformed** (truncated, wrong prefix, surrounding quotes that shouldn't be there) — ask before writing it to prod.
- **Pushing to GitHub or modifying external infra outside the explicit deploy flow** — confirm first, even with broad "do what you need" authorization. STEP 24's `gh pr merge` is in-scope; anything else (force-push, branch delete, repo settings) is not.

---

## MULTI-TENANT VARIANT

If the prod Mac already runs a shared `gateway-nginx` container that owns :80/:443 and routes by Host header to other apps, this app does NOT publish host ports. Deltas:

**STEP 3 — `docker-compose.prod.yml` shape changes:**
```yaml
services:
  web:
    container_name: <project>-web      # nginx upstream resolves by name
    ports: !reset []                   # explicit reset — overlay merges otherwise
    volumes: !reset []
    expose:
      - "<APP_INTERNAL_PORT>"          # internal only, no host bind
    networks: [default, web-gateway]
    restart: always

networks:
  web-gateway:
    external: true                     # gateway-nginx lives on this network
```

The `web-gateway` network must already exist on the prod Mac (first app on the box creates it: `docker network create web-gateway`). Subsequent apps just join. STEP 6.1's bootstrap should add an idempotent `docker network inspect web-gateway >/dev/null 2>&1 || docker network create web-gateway`.

**STEP 5 — Cloudflare tunnel ingress points at the gateway, not the app:**
```yaml
# ops/cloudflared.config.yml
ingress:
  - hostname: <PROD_HOSTNAME>
    service: http://localhost:80       # gateway-nginx, not the app
  - service: http_status:404
```
The tunnel still exists per-app (each hostname is its own tunnel), but every tunnel forwards to `localhost:80`. gateway-nginx demuxes by Host header.

**STEP 6 — add a gateway-nginx site conf (call it STEP 6.4.5):**
```bash
# Render per-app conf and drop into gateway's conf.d
GATEWAY_CONF_DIR="/Users/$PROD_USER/GIT/gateway/nginx/conf.d"   # adjust to your operator's path
cat > "/tmp/${PROJECT_NAME}.conf" <<EOF
upstream ${PROJECT_NAME}_upstream {
  resolver 127.0.0.11 valid=10s;            # Docker's embedded DNS
  server ${PROJECT_NAME}-web:${APP_INTERNAL_PORT};
}

server {
  listen 80;
  server_name ${PROD_HOSTNAME};

  location / {
    proxy_pass http://${PROJECT_NAME}_upstream;
    proxy_set_header Host \$host;
    proxy_set_header X-Real-IP \$remote_addr;
    proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto \$http_x_forwarded_proto;   # Cloudflare sets this
  }
}
EOF

scp "/tmp/${PROJECT_NAME}.conf" "$PROD_SSH:$GATEWAY_CONF_DIR/"
ssh "$PROD_SSH" "docker exec gateway-nginx nginx -t && docker exec gateway-nginx nginx -s reload"
```

Avoid `proxy_set_header Upgrade ...` / `Connection $connection_upgrade` unless the shared `nginx.conf` actually defines `$connection_upgrade` in `http {}`. Most don't, and an undefined variable kills the whole config reload.

**STEP 21 — drift check additions for multi-tenant:**
- Confirm `gateway-nginx` is running: `ssh "$PROD_SSH" "docker ps --filter name=gateway-nginx --format '{{.Status}}'"`.
- Confirm this app's container is on the `web-gateway` network: `ssh "$PROD_SSH" "docker inspect ${PROJECT_NAME}-web --format '{{json .NetworkSettings.Networks}}' | jq 'keys'"` should include `web-gateway`.

**Note for app-stack gotchas (Prisma, Auth.js, Next.js security headers, etc.):**
Those belong in `/3_dev` scaffolding or per-project `CLAUDE.md`, not here. `/9_deploy` is deployment-architecture. Cross-references worth keeping in your project template:
- `prisma migrate dev` for schema changes (never `prisma db push` against shared DBs).
- Pin Prisma CLI version (`prisma@5`, etc.) — `npx prisma` resolves latest from registry and breaks on majors.
- Ship Prisma CLI in the runner image with `RUN ln -sf ../prisma/build/index.js node_modules/.bin/prisma` (Docker `COPY` dereferences the `.bin/` symlink and breaks the CLI's relative wasm load).
- `AUTH_URL` exact match, no trailing slash.
- `NEXT_PUBLIC_*` audit on every PR — those env reads ship to the client bundle.

---

## TROUBLESHOOTING

**Preflight fails on `CLOUDFLARE_API_TOKEN`:**
- Cloudflare dashboard → My Profile → API Tokens → "Create Token"
- Required scopes:
  - Account → Cloudflare Tunnel → Edit
  - Zone → DNS → Edit (for your zone)
- Add to `~/.zshrc`: `export CLOUDFLARE_API_TOKEN=...`

**Preflight fails on `gh auth status`:**
- `gh auth login --scopes repo,workflow,admin:repo_hook`
- If gh is missing: `brew install gh`

**SSH to prod fails:**
- `ssh-copy-id <user>@<prod-mac>` to push your key
- Confirm the prod Mac's "Remote Login" is enabled: System Settings → General → Sharing
- Add a host alias to `~/.ssh/config` for cleaner invocation:
  ```
  Host brice-prod
    HostName 192.168.1.50
    User kevinbrice
    IdentityFile ~/.ssh/id_ed25519
  ```

**Tunnel created but public URL doesn't respond:**
- Note: `dig +short app.brice.com CNAME` returns empty for proxied records — Cloudflare flattens the CNAME to edge IPs at the wire. Don't trust dig to tell you the CNAME is wrong. Use the API:
  ```
  curl -fsS -H "Authorization: Bearer $CLOUDFLARE_API_TOKEN" \
    "https://api.cloudflare.com/client/v4/zones/$ZONE_ID/dns_records?name=app.brice.com&type=CNAME" \
    | jq -r '.result[0].content'
  # Expect: <tunnel_id>.cfargotunnel.com
  ```
- Verify the record is proxied (`"proxied": true` in the API response, orange cloud in the dashboard)
- Check the tunnel actually started on the prod Mac:
  `ssh prod-mac "sudo launchctl list | grep cloudflared"`
- `cloudflared tunnel info <id>` shows tunnel connection count — should be ≥1

**Deploy queues forever after merge:**
- Likely runner label mismatch. Check the runner:
  `gh api /repos/$GH_REPO/actions/runners --jq '.runners[].labels[].name'`
- Must include `self-hosted`, `macos`, `prod`.

**Smoke fails, rollback also fails:**
- Site was probably broken before this deploy. SSH in, `docker compose -f docker-compose.prod.yml down`, check `docker compose logs`, fix forward.

**Public hostname returns 502:**
- Tunnel is up but service behind it is down.
- `ssh prod-mac "docker compose -f docker-compose.prod.yml ps"` — are containers healthy?
- `ssh prod-mac "curl -fsS http://localhost:<port>"` — does the service respond locally?
- If yes → cloudflared config issue. Check `~/.cloudflared/config.yml` on prod.
- If no → app issue, check `docker compose logs <service>`.

**Health endpoint returns old version after successful deploy:**
- Frontend bundle: VITE_APP_VERSION not threaded through Dockerfile ARG / compose `args:`.
- Backend: APP_VERSION not in compose `environment:` block.
- Deploy workflow's "Compute build version" step didn't append to `$GITHUB_ENV`.

**Drift check keeps flagging the runner offline:**
- The Mac probably rebooted without auto-login. Either enable auto-login (STEP 7) or reinstall the runner as a LaunchDaemon: `sudo ./svc.sh install` in `~/actions-runner`.

---

**Start:** STEP 1: DETECT MODE
