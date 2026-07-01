# Mahlanya RPG — Capsule Art & Store Image Specifications

Art direction brief and technical requirements for all store-facing imagery across Steam, iOS App Store, and Google Play.

---

## Steam Image Requirements

All Steam images must comply with Valve's current partner documentation. Sizes below are exact requirements as of the Steam partner documentation (2024–2025 cycle).

| Asset | Dimensions | Format | Notes |
|---|---|---|---|
| Header Capsule | 460×215 px | JPG or PNG | Main store page hero shown at the top of the game's store page |
| Small Capsule | 231×87 px | JPG or PNG | Used in search results, "More like this", and recommendation rows |
| Main Capsule | 616×353 px | JPG or PNG | Featured and Recommended section on the Steam homepage |
| Vertical Capsule | 748×1328 px | JPG or PNG | New on Steam shelf and vertical display contexts |
| Page Background | 1438×810 px | JPG or PNG | Decorative backdrop behind the store page body |
| Library Header | 3840×1240 px | JPG or PNG | Hero banner displayed in the Steam library for this game |
| Library Logo | 1280×720 px | PNG (transparency supported) | Game name / logo overlay on the library header; rendered separately |
| Screenshot | Minimum 5 required; 1920×1080 preferred | JPG | No black bars, no letterboxing; must show actual gameplay or in-game environments |
| Trailer | 1920×1080, H.264, stereo audio | MP4 | Gameplay must be visible within the first 10 seconds; no extended logos or legal splash screens at the start |

**Valve policy notes:**
- Capsule images must not contain review scores, award logos, discount badges, or limited-time offer messaging.
- Capsule images should not contain primarily text. The game logo is permitted; body copy is not.
- All capsule assets must be submitted through the Steamworks partner portal before the page goes live.
- Animated capsules (APNG/WebP) are not currently supported for header or small capsule slots.

---

## iOS Screenshot Requirements

All iOS screenshots are submitted through App Store Connect. Apple requires exact pixel dimensions matching the target device's logical resolution at 3x scale.

| Device Class | Canvas Size | Required? |
|---|---|---|
| iPhone 15 Pro Max (6.7-inch) | 1290×2796 px | Required — primary screenshot set |
| iPhone 14 (6.1-inch) | 1170×2532 px | Optional — falls back to 6.7-inch set if omitted |
| iPad Pro 12.9-inch (6th gen) | 2048×2732 px | Required if the app supports iPad |
| iPad Pro 11-inch (4th gen) | 1668×2388 px | Optional — falls back to 12.9-inch set if omitted |

**App Icon:**
- Size: 1024×1024 px
- Format: PNG
- Alpha channel: not permitted (must be fully opaque)
- Rounded corners: do not add — Apple applies the mask automatically in all contexts
- Text at very small sizes: avoid — the icon must read as a clear silhouette or shape at 29×29 pt
- Submit only the 1024×1024 master; App Store Connect generates all required derivative sizes

**App Store screenshot policy notes:**
- Screenshots may include device frames if Apple-approved frames are used, or may be frameless.
- Up to 10 screenshots per device class are permitted.
- First screenshot is the most prominent in search and browse contexts — it must communicate the game's core identity without requiring the viewer to read the subsequent screenshots.
- Portrait orientation is strongly recommended for mobile; landscape is acceptable for iPad.

---

## Google Play Image Requirements

All Google Play assets are submitted through the Google Play Console.

| Asset | Dimensions | Notes |
|---|---|---|
| Feature Graphic | 1024×500 px | Required for any featured placement on Google Play; shown at the top of the store listing when a trailer is not playing |
| Phone Screenshot | Minimum 1080 px on the short edge; 16:9 aspect ratio preferred | Minimum 2 screenshots required; maximum 8; JPEG or PNG |
| Tablet Screenshot | Minimum 1080 px on the short edge | Recommended for tablet-optimised listings; separate slot from phone |
| Icon | 512×512 px | PNG; no alpha channel; no rounded corners (Google applies the mask) |
| TV Banner | 1280×720 px | Required only if the app targets Android TV; not applicable unless TV support is declared in the manifest |

**Google Play policy notes:**
- The Feature Graphic must not contain misleading imagery or pricing/discount text.
- Screenshots must reflect actual gameplay or UI — no mockups that materially misrepresent the product.
- Trailers linked from YouTube must be public or unlisted, not private.

---

## Art Direction Brief

### Visual Identity

**Primary palette:**
- Laterite red: `#8B3A2A` — the iron-rich earth of Eswatini's Highveld and Middleveld
- Granite grey: `#6B7A8D` — the bare rock faces of the Lubombo and Mdzimba ranges
- Highveld gold: `#C9A84C` — dry winter grass, thatch, and late-afternoon lowveld light
- Night blue: `#1A1E2E` — the deep sky of the Swazi Highveld at altitude, far from any city

**Secondary / accent:**
- The red-and-black Swazi shield (Ihawu) geometric pattern serves as a framing or border motif on capsule and promotional imagery where visual rhythm is needed. The pattern must be used with cultural accuracy — black and red vertical bands with characteristic notch geometry — not as a generic "African pattern".
- White is used sparingly for text legibility only, never as a dominant field colour.

**Typography on capsule art:**
- The game logo only — no body text, no taglines, no descriptive copy on any capsule image. This is both a Valve guideline and a cross-platform best practice. The logo must be legible at the smallest capsule size (231×87 px, Small Capsule).
- The logo should be composed in a way that reads as a silhouette at extreme reduction. The word "Mahlanya" must be recognisable at 87 px height.

**Overall mood and positioning:**
- Historical Africa, serious RPG. Not safari. Not cartoon. Not Western-style fantasy with African aesthetic.
- Reference touchstones: the atmosphere of large-format documentary photography from southern Africa (e.g., David Goldblatt's Highveld landscapes), not film posters for "Lion King" or "Black Panther".
- The imagery should communicate weight, age, and a land that has a deep history — the red earth tells you it has been rained on for thousands of years, the cattle tell you there is an economy and a society, the figure tells you there is a protagonist with agency.

---

## Capsule Key Art Composition

**Mahlanya figure:**
- Silhouette or 3/4 back profile preferred for the primary capsule (avoids the "posed hero" cliché).
- Umshiza fighting stick visible over the shoulder or in hand — a distinctive cultural marker.
- Traditional Swazi dress: isigcebhe chest plate, emahiya wrap — no anachronistic elements.
- The figure should convey forward motion or vigilance, not a static "warrior pose".

**Background:**
- Option A (landscape): The Highveld escarpment dropping sharply toward the Lowveld, photographed or rendered from the edge. Approximately 400 m of visible drop. Acacia thornveld visible in the mid-ground.
- Option B (aerial): A bird's-eye 3/4 view of a Swazi umuti homestead showing the circular hut arrangement radiating from the sibaya cattle kraal at centre. Smoke from cooking fires. Cattle in the kraal.
- For the primary (Header Capsule and Vertical Capsule), Option A is preferred for its sense of scale. Option B is preferred for the Library Header where more canvas is available to establish the game world.

**Logo placement:**
- Lower third of the frame.
- Sufficient clearance from bottom edge to survive any UI overlay from store pages.
- Logo must remain inside the inner 80% safe zone (see Safe Zones below).

---

## Screenshot Shot List (Minimum 7 Shots Required)

At least 5 screenshots must be submitted to all platforms. The following 7 shots are the planned production list, in priority order.

**Shot 1 — Terrain wide shot (Priority: 1)**
The player character visible as a small silhouette on the Highveld-to-Lowveld escarpment at golden hour (approximately 16:30–17:00 in-game time). Cattle are visible as a herd in the valley below. The dramatic elevation change of Eswatini's terrain zones must be legible in a single frame. No HUD visible. This establishes the game world scale.

**Shot 2 — Settlement Voronoi view (Priority: 2)**
Aerial or steep 3/4 overhead view of a Swazi umuti homestead showing the characteristic hut arrangement radiating from the central sibaya cattle kraal. The Voronoi-based procedural generation should be evident in the organic but structured layout. This shot communicates the settlement simulation system and the cultural accuracy of the homestead design.

**Shot 3 — Cultural encounter (Priority: 3)**
Mahlanya performing an inhlonipho greeting before a sikhulu elder seated outside the indlunkulu great hut. The subtitle overlay must be visible showing bilingual text in the format "Sawubona. (I see you.)" The subtitle system is a key feature differentiator and must appear in at least one screenshot.

**Shot 4 — Lightning and weather (Priority: 4)**
A dramatic storm cell over the Lubombo plateau. A lightning strike illuminates a large-canopy acacia and the surrounding savanna. This shot demonstrates the dynamic weather simulation (Microclimate Engine). Best submitted as the trailer's opening hook as well.

**Shot 5 — Co-op cattle raid (Priority: 5)**
Two player characters driving a cattle herd across a river crossing in dynamic weather (light rain or overcast). Player nameplates or co-op indicators may be subtly visible but must not dominate the frame. This communicates the multiplayer/co-op feature and the cattle economy system.

**Shot 6 — Economy and HUD (Priority: 6)**
Close view on the active HUD, showing the cattle count indicator, the political map panel, and the drought severity indicator. The siSwati-first labels should be legible (e.g., "Tinkomo: 47"). This communicates the game's systems depth for players who scan screenshots looking for feature confirmation.

**Shot 7 — Historical event: Mfecane period (Priority: 7)**
A large group of displaced people (refugee column) moving across the Middleveld in dramatic side-lit conditions. Smoke in the distance. A ruined homestead visible in the background. This shot establishes the historical narrative weight and the 1815–1840 Mfecane period as a lived event in the game world, not just a loading-screen fact.

---

## Safe Zones

The following rules apply to all capsule images and screenshots submitted to any store platform.

**Primary safe zone (all stores):**
- All logo art, text, and primary subject matter must remain within the inner 80% of the frame.
- This means a 10% margin on each side (left, right, top, bottom) must be treated as potentially cropped or obscured by store UI chrome, device bezels, or rounded corner masks.
- On a 460×215 px header capsule: the safe zone is 46 px from each edge, leaving an inner canvas of 368×172 px.
- On a 748×1328 px vertical capsule: the safe zone is 75 px from each side and 133 px from top and bottom, leaving an inner canvas of 598×1062 px.

**Small Capsule (231×87 px) — additional constraint:**
- At this size, only the game logo should be legible. No body copy, no subtitle, no tagline, no character detail.
- The logo must be tested at actual pixel size (231×87 rendered at 100% zoom) before delivery. If the wordmark is not readable, the logo treatment must be simplified.
- The primary subject of the key art (landscape, character silhouette) serves only as a colour field at this size — it must not be confusing or visually cluttered.

**Library Header (3840×1240 px) — extended safe zone:**
- Steam overlays the game name and action buttons in the lower-centre region of the library header.
- The primary focal point (key figure, logo) must be positioned in the left third or right third of the frame, not centred.
- The inner 80% rule still applies for the logo; additionally leave the lower-centre 600×200 px region clear of any art intended to be read by the viewer.

---

## File Naming Convention

All submitted assets should follow this naming scheme before upload to the respective store portals:

```
mahlanya_{platform}_{slot}_{dimensions}.{ext}

Examples:
mahlanya_steam_header_capsule_460x215.jpg
mahlanya_steam_vertical_capsule_748x1328.jpg
mahlanya_steam_library_header_3840x1240.jpg
mahlanya_ios_iphone67_screenshot_01_1290x2796.jpg
mahlanya_ios_ipad_pro_12_screenshot_01_2048x2732.jpg
mahlanya_android_feature_graphic_1024x500.jpg
mahlanya_android_phone_screenshot_01_1080x1920.jpg
mahlanya_android_icon_512x512.png
```

---

## Delivery Checklist

Before any store submission milestone, verify the following:

- [ ] All Steam capsule sizes present and pixel-exact
- [ ] Steam header capsule has no review scores, award badges, or discount text
- [ ] Steam library logo PNG has correct alpha on the logotype boundary
- [ ] Minimum 5 Steam screenshots provided; 7 preferred per this spec
- [ ] Steam trailer shows gameplay within first 10 seconds
- [ ] iOS 1024×1024 icon has no alpha channel
- [ ] iOS screenshots cover iPhone 15 Pro Max (6.7-inch) at minimum
- [ ] iPad screenshots provided if iPad is a declared supported device
- [ ] Google Play feature graphic provided (1024×500)
- [ ] Google Play icon PNG is fully opaque (no alpha)
- [ ] All assets tested in-context at actual store display sizes (browser zoom 100%)
- [ ] Logo legibility confirmed at 231×87 (Small Capsule)
- [ ] All primary subject matter and logo inside inner 80% safe zone on all assets
- [ ] Swazi shield pattern used with accurate geometry if present (red/black vertical band with notch — not a generic pattern)
- [ ] No anachronistic elements visible in any screenshot (post-1906 objects, modern clothing, UI chrome from test builds)
