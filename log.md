# Improvements and Changes

## Roadmap

This section describes the general things to implement.

### Search

- [x] **negamax** with alpha-beta pruning
- [x] **quiescence search**
- [x] **iterative deepening** framework
- [x] **null move pruning**
- [x] **killer moves**
- [x] **MVV/LVA** move ordering
- [x] **transposition table**
- [ ] **late move reductions**
- [ ] **futility pruning**

### Evaluation

**Part 1** HCE

- [x] **piece** value
- [x] **mobility**
- [ ] **king safety**
- [ ] passed **pawns**
- [ ] **pawn** structure
- [ ] **threats**
- [x] **space**

**Part 2** NNUE transition

- [ ] **NNUE** data generation
- [ ] **NNUE** training
- [ ] **NNUE** evaluation support 

### Engine

- [ ] **opening book**
- [x] basic **UCI** support
- [ ] **options**
- [ ] **syzygy** endgame tablebase