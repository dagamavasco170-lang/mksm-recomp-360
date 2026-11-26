# Bounded IOMAN metadata trace

IomanTraceRing implements IomanTraceSink with fixed memory and no heap allocation in the callback. It retains the newest 256 events, counts overwritten events, and captures operation, bounded path text (Open only), descriptor, arguments and result. It never captures file payload.

Attach it to IomanDispatcher with its optional trace-sink constructor argument. Read records in chronological order with at(index). Export/CSV/JSON conversion belongs on the host/debug side so the console trace path avoids filesystem writes.

The dropped() counter reports records overwritten after capacity. clear() resets both the ring and overflow count. Trace output can be fed to the existing metadata correlator once exported as JSON/CSV.
