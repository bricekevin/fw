# ADR 002: Database Choice

**Date:** [DATE]  
**Status:** Accepted  
**Deciders:** [NAMES_OR_ROLES]  
**Related:** [ADR_001_TECH_STACK]

---

## Context

[Describe the data requirements that drive this decision]

**Example:**
> The system needs to store [DATA_TYPES]. Key requirements include:
> - Data complexity: [SIMPLE/MODERATE/COMPLEX/GRAPH]
> - Expected scale: [USER_COUNT or DATA_VOLUME]
> - Query patterns: [READ_HEAVY/WRITE_HEAVY/BALANCED]
> - Relationships: [FEW/MODERATE/MANY]
> - Real-time requirements: [YES/NO]

---

## Decision

We will use **[DATABASE_NAME]** ([RELATIONAL/DOCUMENT/KEY-VALUE/GRAPH]) as our primary database.

### Configuration
- **Version:** [VERSION]
- **Deployment:** [MANAGED_SERVICE/SELF_HOSTED]
- **Hosting:** [WHERE]

### Schema Strategy
- [SCHEMA_APPROACH: NORMALIZED/DENORMALIZED/HYBRID]
- [MIGRATION_STRATEGY]

---

## Alternatives Considered

### Option 1: [DATABASE_NAME]
**Type:** [TYPE]

**Pros:**
- [BENEFIT_1]
- [BENEFIT_2]

**Cons:**
- [DRAWBACK_1]
- [DRAWBACK_2]

**Rejected because:** [REASON]

### Option 2: [DATABASE_NAME]
**Type:** [TYPE]

**Pros:**
- [BENEFIT_1]
- [BENEFIT_2]

**Cons:**
- [DRAWBACK_1]
- [DRAWBACK_2]

**Rejected because:** [REASON]

---

## Consequences

### Positive
- ✅ [BENEFIT_1]
- ✅ [BENEFIT_2]
- ✅ [BENEFIT_3]

### Negative
- ❌ [TRADE_OFF_1]
- ❌ [TRADE_OFF_2]

### Neutral
- ℹ️ [CONSIDERATION_1]
- ℹ️ [CONSIDERATION_2]

---

## Data Model

### Core Entities
```
[ENTITY_1] (id, field1, field2)
[ENTITY_2] (id, field1, field2)
[ENTITY_3] (id, field1, field2)
```

### Relationships
```
[ENTITY_1] --[RELATIONSHIP]--> [ENTITY_2]
```

---

## Performance Considerations

**Expected Load:**
- Read queries: [ESTIMATION_QPS]
- Write queries: [ESTIMATION_QPS]
- Peak concurrent users: [COUNT]

**Scaling Strategy:**
- [VERTICAL/HORIZONTAL/BOTH]
- [SHARDING_STRATEGY_IF_APPLICABLE]
- [CACHING_STRATEGY]

---

## Backup & Recovery

**Backup Frequency:** [SCHEDULE]  
**Retention Period:** [DURATION]  
**Recovery Time Objective (RTO):** [TIME]  
**Recovery Point Objective (RPO):** [TIME]

---

## Migration Considerations

**From:** [PREVIOUS_DATABASE_IF_APPLICABLE]  
**Migration Strategy:** [APPROACH]  
**Rollback Plan:** [STRATEGY]

---

## References

- [DATABASE_DOCUMENTATION]
- [PERFORMANCE_BENCHMARKS]
- [MIGRATION_GUIDES]

---

**Last Updated:** [DATE]

