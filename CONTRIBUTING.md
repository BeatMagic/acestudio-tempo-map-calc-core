# Contributing

This repo publishes the tempo-curve math ACE Studio runs, so other tools can line up on the same
curve. That is its purpose: something to read, embed, and depend on.

## Issues

Issues are the channel we watch, and the most useful thing you can send. Questions about the model,
use cases we have not thought of, and numbers that look wrong all help. If something here is unclear
or inaccurate, we would rather hear about it than not.

## Before writing code

These files are shared source: ACE Studio compiles them too, so the core moves with the product rather
than on its own. If you are considering a change, open an issue first and we can point you in the
right direction.

Documentation corrections are simpler, and welcome.

## Ports to other languages

Welcome, and they tend to work best as their own project. Keeping a port in your own repo lets it
follow your language's conventions and release on its own schedule, which someone fluent in that
language will do better than we would from here.

[docs/porting.md](docs/porting.md) has the recipe and the traps, and
[the shared fixture](fixtures/tempo_curve_cases.json) is how you show the port agrees with ACE Studio.
Tell us about it in an issue once it works and we are happy to point people to it. If supporting your
language needs something from this side, say so there too and we can work out how together.

---

Release and maintenance mechanics live in [MAINTAINERS.md](MAINTAINERS.md).
