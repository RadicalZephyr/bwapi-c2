+++
title = "Reference"
description = "The facts: every function, constant family, struct and error code, generated from api.json."
sort_by = "weight"
weight = 3
+++

The reference states what is, and nothing else: for every exported function its C name, its
parameters and their handle kinds, its return kind and the neutral value it returns on an invalid
handle, when it latches an error, and where it departs from the C++ it wraps. One page per
function, so every symbol has a stable URL and the search box finds it. Tables for each constant
family, each struct that crosses the boundary with its field offsets and sizes, and the error
codes. One hand-written page for the frame loop, the ABI's one protocol.

> **Unstable.** The ABI is pre-1.0. Signatures may change until the consumer phase completes;
> after 1.0 every entry carries a `since` badge and nothing is ever removed.

The pages are generated at build time from `api.json`, the same file binding generators consume,
and are not checked in. Until the generator lands, this section holds only this page.

What does not belong here: guidance. A reference entry says what a function does; how to use it
well is a how-to guide, and why it works that way is an explanation.
