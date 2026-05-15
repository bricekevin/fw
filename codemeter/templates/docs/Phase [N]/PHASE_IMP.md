# Phase [N] - [PHASE_NAME] Implementation

**Status:** [IN_PROGRESS | TESTING | COMPLETE]  
**Checkpoint:** [v0.X.0]

---

## Setup

```bash
# Environment setup
cp .env.example .env

# Database migration
[MIGRATION_COMMAND]

# Install dependencies
[INSTALL_COMMAND]
```

---

## Implementation

### Database

**Migration:**
```sql
CREATE TABLE [table] (
  id UUID PRIMARY KEY,
  [field] [type]
);
```

**Rollback:**
```sql
DROP TABLE [table];
```

---

### Backend

**File:** `backend/[path]/[file]`

```[language]
// [COMPONENT_IMPLEMENTATION]
[CODE_SNIPPET]
```

**Tests:**
```[language]
// [TEST_CODE]
[TEST_SNIPPET]
```

---

### Frontend

**Component:** `src/[path]/[Component].tsx`

```typescript
export const [Component] = () => {
  // [IMPLEMENTATION]
};
```

---

### API

**Endpoint:** `[METHOD] /api/v1/[resource]`

```[language]
// [ENDPOINT_IMPLEMENTATION]
[CODE]
```

---

## Testing

```bash
# Run tests
[TEST_COMMAND]

# Verify
[VERIFY_COMMAND]
```

---

## Deployment

```bash
# Build
[BUILD_COMMAND]

# Deploy
[DEPLOY_COMMAND]

# Health check
curl [HEALTH_ENDPOINT]
```

---

## Rollback Plan

### Emergency Rollback Procedure

**If this phase breaks production, follow these steps:**

#### 1. Immediate Response (< 5 minutes)
```bash
# Revert to previous stable version
git log --oneline -10  # Find last stable commit
git checkout [STABLE_COMMIT_HASH]

# Or revert specific commits
git revert [COMMIT_1] [COMMIT_2] [COMMIT_3] --no-edit

# Push rollback
git push origin main
```

#### 2. Database Rollback (if applicable)
```bash
# List migrations
[MIGRATION_LIST_COMMAND]  # e.g., npm run migrate:list

# Rollback this phase's migrations
[MIGRATION_ROLLBACK_COMMAND]  # e.g., npm run migrate:rollback -- --step=3

# Verify database state
[MIGRATION_STATUS_COMMAND]  # e.g., npm run migrate:status
```

#### 3. Service Restart
```bash
# Restart services
[RESTART_COMMAND]  # e.g., docker-compose restart

# Or full rebuild
[REBUILD_COMMAND]  # e.g., docker-compose down && docker-compose up -d

# Verify services healthy
[HEALTH_CHECK_COMMAND]  # e.g., curl http://localhost:3000/health
```

#### 4. Verification (< 10 minutes)
```bash
# Run smoke tests
[SMOKE_TEST_COMMAND]  # e.g., npm run test:smoke

# Check logs for errors
[LOG_CHECK_COMMAND]  # e.g., docker-compose logs --tail=50

# Verify critical endpoints
curl [CRITICAL_ENDPOINT_1]
curl [CRITICAL_ENDPOINT_2]

# Monitor for 10 minutes
[MONITORING_COMMAND]  # e.g., tail -f logs/app.log
```

#### 5. Communication
```bash
# Post-rollback checklist:
- [ ] Notify team in Slack/Discord
- [ ] Update status page if public-facing
- [ ] Document incident in HANDOFF_NOTES.md
- [ ] Create post-mortem issue
- [ ] Schedule fix and re-deployment
```

### Rollback Testing

**Test rollback procedure before production deployment:**

```bash
# In staging environment
1. Deploy phase to staging
2. Test features work
3. Execute rollback procedure above
4. Verify staging returns to previous state
5. Document any issues found
```

**Rollback tested in staging:** [ ] YES / [ ] NO  
**Rollback tested date:** [DATE]  
**Tested by:** [NAME]

### Known Rollback Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| [RISK_1] | [HIGH/MED/LOW] | [STRATEGY] |
| [RISK_2] | [HIGH/MED/LOW] | [STRATEGY] |

**Example risks:**
- Data loss if new columns contain critical data
- Breaking changes in API contracts
- Cache inconsistencies after rollback

---

## Completion Checklist

- [ ] All features implemented
- [ ] Tests passing
- [ ] Documentation updated
- [ ] Deployed to staging
- [ ] **Rollback procedure tested** ✨ NEW
- [ ] Ready for production

---

**Updated:** [DATE]

