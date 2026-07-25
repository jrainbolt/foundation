# Foundation Vision

## Vision

Foundation is a reusable simulation engine for automation, logistics, colony
building, and RTS-style experiences. It exists to provide a small, dependable
core on which many games and experiments can be built, rather than embedding
one game's rules into the engine.

## Design Philosophy

The simulation owns authoritative state and rules. Rendering observes that
state through a narrow interface but cannot change timing or outcomes. This
separation permits console tools, graphical frontends, tests, and servers to
run the same simulation.

Foundation favors deterministic fixed ticks, explicit ownership, standard C17,
and small modules with focused APIs. Scenario rules should compose engine
capabilities instead of becoming assumptions inside them.

## Long-Term Goals

- Support multiple scenarios, including automation, survival, restoration,
  colony building, tower defense, and sandbox play.
- Produce repeatable outcomes from the same initial state and inputs.
- Remain portable across graphical, console, testing, and headless frontends.
- Grow through independently testable systems with explicit update ordering.

## Non Goals

- Defining one canonical game or campaign.
- Coupling engine state to a renderer, UI framework, or input library.
- Implementing scenario gameplay in the architectural foundation.
- Adding systems before their ownership and deterministic order are clear.
