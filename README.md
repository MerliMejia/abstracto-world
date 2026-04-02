# abstracto-world

Domain helpers for terrain and world-surface authoring.

Current scope:

- Regular heightfield grids defined over `x,z`
- CPU-side height queries from world coordinates
- CPU-side mesh generation for rendering adapters

This module stays renderer-agnostic. Rendering and editor integration belong in `abstracto-engine`.
