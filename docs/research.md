# Deterministic research and Research Labs

The global `FactoryResearchState` remains authoritative for active selection,
per-technology unit/work progress, committed science, completed technologies,
prerequisites, and unlock flags. A Research Lab owns only its entity identity,
position, bounded Basic Science inventory, and latest-tick observations. Labs
never own technology progress or completion state.

Normal gameplay research is physical: storage and belts carry Basic Science,
inserters deliver it through `FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT`, and a
powered lab consumes the complete unit cost before contributing one work tick.
The lab accepts only Basic Science and holds at most 100 items. Its fixed,
indivisible power demand is 50 units every connected tick, including idle
ticks. Inserter delivery occurs after research, so delivered science becomes
usable on the following tick.

At most one lab contributes each simulation tick. Eligibility and selection
use ascending stable entity ID, independent of component-store order. When a
new unit needs science, the selected lab must contain the full unit cost. Once
consumed, that science belongs to global progress: any powered lab, including
an empty one, may continue the committed unit. Selection changes, demolition,
power loss, and snapshots do not refund or reassign committed science.

Activity precedence is `UNPOWERED`, `NO_ACTIVE_RESEARCH`, `NO_SCIENCE`, then
`WORKING` for the selected lab and `WAITING` for other eligible labs. Latest
science consumed and work contributed reset each successful tick and are not
serialized. `RESEARCH_UNIT_COMPLETED` identifies the lab contributing the
completion tick; its science fields describe the unit cost and do not claim
that this same lab originally consumed it.

There is no controller-owned unconsumed science inventory and no command for
injecting science directly into research. Test and demo setup seed ordinary
item endpoints; all research consumption occurs at a physical lab.

Snapshot version 17 stores lab ID, position, and inventory. Global metadata
stores only the active technology, completion bits, and per-technology unit,
work, and commitment progress. Derived power topology, activity, and
latest-tick values are rebuilt or reset.
