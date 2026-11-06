> Superseded by [build-guide.md](../build-guide.md) (7 Nov 2026). Kept for history; converted from the original .docx.

## F1 Dashboard – Task Breakdown

Version 1 – written 7 October 2026. Built from the spec (Version 2). This is a living document: tick things off and change it as the build goes.

### How to use this

Each job builds one small thing that works on its own, so there is always something real to look at on the boards. Do the jobs in order. The developer does the hands-on work; any job marked "the maker checks" needs the maker to look at it and say yes before moving on.

Every job has three parts:

- Do: what to build.

- Done when: how you can tell it works.

- Needs: which jobs must be finished first.

After each stage there is a checkpoint: stop, test everything so far on the real boards, and only then move on.

### Stage 1 – Get the boards going

#### Job 1. Hello on all three screens

- Do: set up Arduino IDE 1.8.19 (ESP32S3 Dev Module, USB CDC On Boot = Enabled, PSRAM = OPI PSRAM). Upload LCDwiki's demo to each board, then change it to show the words "Standings", "Map" and "Drama".

- Done when: each board shows its own name.

- Needs: nothing.

#### Job 2. Touch test

- Do: make each screen draw a dot where you touch it.

- Done when: the dot lands under your finger on all three boards.

- Needs: Job 1.

#### Job 3. Wifi and the clock

- Do: each board joins the home wifi and shows New Zealand time.

- Done when: the time is right on all three and survives unplugging and plugging back in.

- Needs: Job 1.

Checkpoint 1: three boards on, touch working, all online with the right time.

### Stage 2 – Live data (the riskiest part, so it goes early)

#### Job 4. First words from OpenF1

- Do: one board asks OpenF1 for the latest race and shows the track name.

- Done when: the right track name appears on screen.

- Needs: Job 3.

#### Job 5. Race replay for testing

- Do: make a "replay" switch that plays a past race from OpenF1 as if it were live, sped up if wanted.

- Done when: a past race can be played back at any time of day. (Races only happen some weekends, so nearly all testing uses this.)

- Needs: Job 4.

#### Job 6. Car positions

- Do: pull the positions of all 22 cars, a few times a second, and keep them in memory.

- Done when: the board keeps up through a whole replayed race without freezing or running out of memory.

- Needs: Job 5.

Checkpoint 2: a full past race streams into a board from start to finish.

### Stage 3 – Map board

#### Job 7. One map on screen

- Do: convert one season map (v4) into the board's picture format and show it.

- Done when: it looks as sharp as on the laptop. The maker checks.

- Needs: Job 1.

#### Job 8. All 23 maps, picked by itself

- Do: convert the other 22 maps. The board picks the right one from the live data.

- Done when: replaying three different races shows the three right tracks.

- Needs: Jobs 4 and 7.

#### Job 9. Test dots that follow the track

- Do: draw a plain dot for each car, lined up so the dots run along the drawn track.

- Done when: on a replay, the dots stay on the track all the way round, on several circuits.

- Needs: Jobs 6 and 8.

#### Job 10. Real car icons

- Do: swap the dots for car icons in team colour with their number, plus a small tyre icon. Fix Cadillac's black car so it shows up.

- Done when: every car is easy to tell apart. The maker checks.

- Needs: Job 9.

#### Job 11. Running order strip and lap count

- Do: thin strip at the edge with the running order and "lap 12 of 58".

- Done when: it matches the real result at the end of a replay.

- Needs: Job 10.

#### Job 12. Podium rings

- Do: gold, silver and electric blue pulsing rings on the top three, swapping on a pass, pulsing faster as gaps close.

- Done when: it looks right through a replayed overtake. The maker checks.

- Needs: Job 11.

#### Job 13. Battles, crashes and the safety car

- Do: battle rings (no more than 4 or 5), red crash ripple, strobing yellow safety-car beacon, 3-letter codes on the top three, labels that nudge apart when cars bunch up.

- Done when: each shows up at the right moment on a replay that had a crash and a safety car. The maker checks.

- Needs: Job 12.

Checkpoint 3: a whole past race plays on the map board and looks like the finished thing.

### Stage 4 – Boards talking to each other

#### Job 14. ESP-NOW link

- Do: the boards send short messages to each other directly, no router needed.

- Done when: a tap on one screen changes the colour of the other two, straight away.

- Needs: Job 2.

### Stage 5 – Standings board

#### Job 15. The race order list

- Do: every driver in order, with tyre and tyre age (worked out by us, not OpenF1's figure), current lap at the top.

- Done when: it matches the real order through a replay. The maker checks the look.

- Needs: Job 6.

#### Job 16. Driver photos and the feature view

- Do: crop the 21 photos to head and shoulders, make a silhouette with 41 for Arvid Lindblad, then build the feature view: top three one at a time, full screen, photo, car, position, gap.

- Done when: it breaks away and comes back to the list smoothly. The maker checks.

- Needs: Job 15.

### Stage 6 – Drama board and sound

#### Job 17. First sound

- Do: wire up the speaker (check whether the in-box one is good enough, otherwise the MAX98357 and small speaker). Play one MP3.

- Done when: the five-light start plays clearly at a sensible volume.

- Needs: Job 1.

#### Job 18. The drama feed

- Do: show current lap, gaps, weather, fastest lap, pit stops, flags and safety car.

- Done when: each event appears as it happens on a replay. The maker checks the layout.

- Needs: Job 6.

#### Job 19. Ryan and the effects

- Do: play the right sound and Ryan line for each big moment, including the crash call for the right corner. Add the volume slider and mute.

- Done when: Ryan calls a crash at the right corner on a replay, and stays quiet the rest of the time.

- Needs: Jobs 17 and 18.

#### Job 20. Incident card

- Do: on a crash, show the driver's face and car with what happened.

- Done when: it pops up on a replayed crash. The maker checks.

- Needs: Jobs 16 and 18.

Checkpoint 4: all three boards run a whole replayed race together, with sound, nobody touching anything. This is the big one.

### Stage 7 – Between races (attract mode)

#### Job 21. The AF1 badge

- Do: design the badge ("AF1", with the user's name underneath when there's room), then the swipe across all three screens on white.

- Done when: the swipe looks like one long screen. The maker checks.

- Needs: Job 14.

#### Job 22. Championship top three

- Do: leader left, 2nd middle, 3rd right, each flipping photo, car, stats card, in step.

- Done when: all three flip together. The maker checks.

- Needs: Jobs 16 and 21.

#### Job 23. Next race and the three countdowns

- Do: next race across the screens, then the next three races each with a live New Zealand-time countdown.

- Done when: the countdowns tick in step and the times are right.

- Needs: Jobs 3 and 21.

#### Job 24. The loop, and snapping to live

- Do: the map board runs the whole sequence on a loop and tells the others when to change. All three snap to live when a session starts.

- Done when: it loops for an hour without going dark, then switches when a replay "starts".

- Needs: Jobs 22 and 23.

### Stage 8 – Touch

#### Job 25. Map taps

- Do: corner names, zoom a section, car panel, layers menu (remembered after power-off), extra layers (grid, rain, DRS, gap line, racing line, codes).

- Done when: every map tap in spec section 8 works.

- Needs: Job 13.

#### Job 26. Pins, halos and follow

- Do: up to four pinned cars with flashing halos from the colour swatch, plus Follow 30 seconds, Stay on and Unfollow.

- Done when: pinning from either board works and halos stay easy to tell apart. The maker checks the colours.

- Needs: Jobs 14 and 25.

#### Job 27. Standings taps

- Do: driver card, race story, tyre age, flip to championship points.

- Done when: every standings tap in spec section 8 works.

- Needs: Job 16.

#### Job 28. Drama taps and the secret

- Do: every drama tap in spec section 8, plus the hidden badge tap with the user's message.

- Done when: each tap works, and the secret is hard to find by accident.

- Needs: Jobs 20 and 21.

### Stage 9 – Practice and qualifying

#### Job 29. Knowing which session it is

- Do: boards tell race, qualifying and practice apart and switch view to match.

- Done when: replays of each type show the right view.

- Needs: Job 24.

#### Job 30. Qualifying

- Do: sectors lighting purple / green / yellow on the map, close-up of the hot-lap car on one side screen, timing and drop zone on the other.

- Done when: a replayed qualifying looks right. The maker checks, and decides which side screen shows which.

- Needs: Job 29.

#### Job 31. Practice

- Do: the calm version: shows it is practice, the times, and who is quickest.

- Done when: a replayed practice looks right. The maker checks.

- Needs: Job 29.

### Stage 10 – Finishing

#### Job 32. Frames and power

- Do: mount the three boards in the matching black frames, run power from the one 4-port charger.

- Done when: it looks like artwork, not a gadget. The maker checks.

- Needs: Job 17 (speaker placement).

#### Job 33. The real test

- Do: leave it running through a real race weekend: practice, qualifying and the race.

- Done when: every "Done when" in spec section 12 is ticked.

- Needs: everything.

Final checkpoint: The user loves it.
