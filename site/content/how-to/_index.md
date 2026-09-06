+++
title = "How-to guides"
description = "A task you already have, done: consuming the DLL, sizing buffers, initialising BWEM, bumping a pin."
sort_by = "weight"
weight = 2
+++

A how-to guide answers one question you arrived with: how do I consume the DLL from C#, how do I
size a buffer for a collection, how do I initialise BWEM and find the natural expansion, how do I
bump a pinned dependency. It assumes you know what you want and roughly what the ABI is; it gives
the steps and stops. Where a choice exists it names it and picks one, and leaves the reasoning to
the Explanation section.

What does not belong here: a first run from nothing (tutorial), the facts about one function
(reference), and why the design is what it is (explanation).

Guides are written as the tasks arrive. Planned first: consuming the DLL from C, Python, C# and
Rust; the retry idiom for sizing buffers; choosing between the sticky error latch and the error
callback; running the differential test against a real StarCraft installation; and the pin-bump
procedure.
