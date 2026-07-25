# Swazi Traditional Astronomical Calendar

Reference document for `historical_accuracy_check.py` and quest writers.
All astronomical assertions are grounded in Swazi oral tradition and verified
against the Eswatini centroid coordinates (lat: -26.5225°, lon: 31.4659°).

---

## isiLimela — The Planting Stars (Pleiades)

The Pleiades open cluster (M45) is called **isiLimela** in siSwati. The word
derives from *limela* — to dig or hoe — making the etymology a direct
agricultural instruction: when these stars appear, it is time to work the
soil.

**Heliacal rising (Southern Hemisphere):** In Eswatini the Pleiades rise
heliacally in **May–June** (austral winter), appearing just before dawn after
a period of invisibility lost in the sun's glare. This is the opposite of the
Northern Hemisphere tradition where Pleiades signal spring planting.

**Cultural role:** The isiLimela rising marks the beginning of planning for
the planting season, which runs October–December (austral spring/summer).
It is also one of the calendrical anchors for **Incwala** ceremony preparation,
though the Incwala itself is tied to the summer solstice (December/January),
not the Pleiades rising directly.

**Visibility window:** Pleiades are above the horizon nightly from roughly
April through November from Eswatini. They are invisible (solar conjunction)
roughly December–March, precisely during the Incwala ceremony period.

**Historical accuracy rule (ANACHRONISM):** Any quest set in
October–December must NOT depict isiLimela as currently rising. The
Pleiades are in solar conjunction during that window and not visible at dusk
or dawn. A character saying "isiLimela rises — it is time to plant" in
December is an anachronism; at that date the stars are already well above the
horizon at nightfall, not rising for the first time.

---

## iNkosana — The Little Chief (Southern Cross / Crux)

The Southern Cross (**Crux**) is called **iNkosana** — "the little chief" or
"the prince." From lat -26.5° it is **circumpolar**: it never sets below the
horizon. Hunters and herdsmen used the Southern Cross axis to locate true
south at night with high accuracy.

**Technique:** The long axis of the cross, extended approximately 4.5 times
its own length, points to the south celestial pole. No bright star marks the
south pole (unlike Polaris in the north), so the Southern Cross was essential
for southern navigation.

**Visibility quality:** The Southern Cross transits highest (best viewing
altitude) during **April–August** (austral autumn/winter). It is always
available for navigation but lowest in the sky during January–February.

**Historical accuracy rule:** Night-travel or navigation scenes must use
iNkosana (Southern Cross) or the belt of iNgonyama (Orion) for direction
references — never Polaris or the Northern Dipper. Eswatini is 26° south;
Polaris sits -26° below the northern horizon and is not visible.

---

## iNgonyama — The Lion (Orion)

Orion is called **iNgonyama** — "the lion" — in the Swazi tradition, evoking
strength and the hunt. In the Southern Hemisphere, Orion appears upside-down
relative to the Northern Hemisphere view: the belt runs from upper-left
(Mintaka) to lower-right (Alnitak) when facing north.

**Visibility from Eswatini:** Orion is prominent in the evening sky from
**May through August** (austral winter). This coincides with the dry season
and the traditional hunting season, cementing the association between
iNgonyama and the hunt.

**Belt as a direction marker:** The belt of Orion rises nearly due east and
sets nearly due west from any latitude. Hunters used this property to orient
during early-evening travel before the Southern Cross rose to useful altitude.

**Historical accuracy rule:** Quest dialogue referencing iNgonyama as a
hunting-season marker is valid only for **May–August**. A scene set in
January mentioning "iNgonyama overhead at midnight, the time for the hunt"
is an anachronism; in January, Orion transits around noon and is not visible
at night from Eswatini.

---

## Traditional Swazi Lunar Calendar

The Swazi calendar is lunisolar: twelve or thirteen lunar months per year,
anchored to agricultural and ceremonial cycles. Gregorian mappings are
approximate; the lunar month drifts ~11 days per solar year.

| Swazi Month     | Approximate Gregorian | Agricultural / Ceremonial marker       |
|-----------------|----------------------|----------------------------------------|
| Bask'olukhulu   | January              | Incwala concludes; harvest begins      |
| Indlovana       | February             | Sorghum harvest                        |
| Inhlaba         | March                | Autumn; cattle movement                |
| Mabasa          | April                | Cool season; firebreak burning         |
| Inkhwekhweti    | May                  | isiLimela rising; field preparation    |
| Inhlolanja      | June                 | Coldest month; isiLimela prominent     |
| Kholwane        | July                 | Mid-winter; iNgonyama high             |
| Ingci           | August               | Warming begins; end of Orion season    |
| Inyandzana      | September            | Early spring; soil preparation         |
| Mphala          | October              | Planting season opens                  |
| Lweti           | November             | Main planting; rains expected          |
| Ingongoni       | December             | Incwala ceremony; summer solstice      |

A 13th intercalary month is added when the lunar calendar drifts too far from
the agricultural season. The exact insertion is determined by the Swazi royal
council, not a fixed algorithmic rule — quest writers must not hard-code it.

---

## Historical Accuracy Rules for `historical_accuracy_check.py`

The following rules are machine-checkable patterns. Each maps to an ERROR or
WARNING severity.

### ERROR — Astronomical Anachronisms

| Pattern in quest text | Rule | Reason |
|-----------------------|------|--------|
| "sunrise in the North" | ERROR | Southern Hemisphere: the sun transits the **northern** sky. Eswatini is at -26.5°; the sun rises in the NE/E/SE depending on season, but its arc is always north of zenith. A character saying "sunrise in the south" is the anachronism to catch. |
| "isiLimela rises" in a quest set Oct–Dec | ERROR | Pleiades are in solar conjunction; not visible at heliacal rise |
| "Polaris" or "North Star" used for navigation | ERROR | Not visible from -26.5° latitude |
| "iNgonyama hunts" in a quest set Nov–Mar | WARNING | Orion not prominent in evening sky during those months |
| iNkosana (Southern Cross) described as "seasonal" | WARNING | Circumpolar from Eswatini — visible every night |

### WARNING — Post-1900 Modernisation Notes

Any quest set **after 1900** that uses traditional star names
(isiLimela, iNkosana, iNgonyama) without a modernisation note in the quest
metadata should emit a WARNING. By 1900, European missionary education and
colonial cartography introduced Greco-Roman star names; a fully traditional
reference in a 1920s quest setting requires justification in the quest
`era_context` field.

### Direction of isiLimela signal

The correct cultural statement is: "isiLimela rises in the east before dawn
in May/June — this means the planting season will come." The incorrect
(anachronistic) form is treating it as a spring/summer signal by Northern
Hemisphere analogy.

---

## Sources and Authority

These descriptions synthesise:
- Swazi oral tradition as recorded in academic ethnography (Kuper 1947,
  *An African Aristocracy*; Beidelman 1966, comparative ritual analysis).
- Southern-Hemisphere archaeoastronomy literature on Pleiades heliacal
  rising timing.
- Positional astronomy verified against JPL Horizons ephemeris at
  lat -26.5225°, lon 31.4659° (Mbabane, Eswatini).

Any addition to this document requires sign-off in the cultural review log
(`docs/cultural-review-protocol.md`) before it may feed the CI gate.
