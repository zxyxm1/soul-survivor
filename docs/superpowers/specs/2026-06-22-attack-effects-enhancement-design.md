# Attack Effects Enhancement Design

**Date:** 2026-06-22  
**Project:** Soul Survivor  
**Status:** Approved

## Goal

Make attack visual effects significantly more noticeable in the terminal ASCII game.

## Current State

- All effects are single characters, disappearing quickly
- Projectiles have no trail; no hit feedback on impact
- Slash animations only appear at enemy position (single rotating char)
- Colors are muted; no BOLD usage on effects

## Design: Particle-Based Effect System (方案 A)

### Architecture

```
Attack occurs
  ├── Player attack flash (p->attack_flash countdown)
  ├── Projectile flight → EFX_TRAIL trail particles behind it
  ├── Hit detected → 4-6 EFX_SPARK particles burst outward
  └── Slash animation preserved + spark burst overlay
```

### File Changes

#### game.h
- Add `EFX_SPARK`, `EFX_TRAIL` to `EffectType` enum
- Increase `MAX_EFFECTS` from 40 → 60
- Add `int attack_flash` to `Player` struct
- Declare: `effects_spawn_spark()`, `effects_spawn_hit_burst()`

#### effect.c
- `effects_spawn_spark()`: spawn 1 spark at position + random offset, weapon-colored, 3-5 frame lifetime
- `effects_spawn_hit_burst()`: spawn 4-6 sparks at hit point for explosion pattern
- Projectile trail: spawn `EFX_TRAIL` at old position each movement step
- Hit feedback: call `spawn_hit_burst()` when projectile reaches target
- Slash enhancement: spawn spark burst on first frame, use BOLD colors
- Spark update: lifetime decrement, color fades bright→dim

#### combat.c
- Melee attack: set `p->attack_flash = 3`
- Ranged attack: set `p->attack_flash = 2`

#### render.c
- style_buf entries for spark chars (`*` `+` `.` `x`) with BOLD + weapon colors
- Trail char (`·`) with DIM color
- Player attack flash: `♥` → BOLD WHITE when `attack_flash > 0`

#### player.c
- `player_init()`: initialize `attack_flash = 0`
- `player_update_cooldowns()`: decrement `attack_flash`

### Weapon Effect Summary

| Weapon | On Attack | Flight | On Hit |
|--------|-----------|--------|--------|
| Stick  | Player white flash | - | Cyan X-slash + white spark burst |
| Bone   | Player white flash | White `\|` + `·` trail | White spark burst |
| Fire   | Player white flash | - | Yellow/red dual spark burst + AoE |
| Knife  | Player white flash | - | Cyan fast spark burst |
| Lightning | Player white flash | Yellow `~` + `·` trail | Yellow spark burst |

### Constraints
- Terminal-safe ASCII/Unicode characters only
- No breaking existing game balance or mechanics
- All changes within `effect.c`, `combat.c`, `render.c`, `player.c`, `game.h`
