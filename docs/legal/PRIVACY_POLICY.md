# Privacy Policy — Mahlanya

**Effective Date:** 2026-07-01

## 1. Overview

Charles Bartaria ("Developer," "we," "us") operates the video game Mahlanya ("Game"). This Privacy Policy describes what information is collected when you play the Game, how it is used, and your rights with respect to that information. By installing or playing the Game, you agree to the data practices described here.

## 2. Data We Collect

### 2.1 Steam Account Identifier

The Game is distributed via the Steam platform operated by Valve Corporation. When you launch the Game through Steam, the Steamworks SDK is initialized and provides the Game with your Steam Account ID (a 64-bit integer). This identifier is used to:

- Associate save game files with your Steam account for Steam Cloud synchronization;
- Track achievement unlock status via the Steamworks Achievements API;
- Enable co-operative multiplayer sessions via Steam Networking.

The Steam Account ID is not a personally identifiable name or email address. It is a numeric identifier specific to the Steam platform.

### 2.2 Local Save Files

The Game stores save data locally on your device in the directories managed by Unreal Engine 5's SaveGame subsystem (typically `%LOCALAPPDATA%\Mahlanya\Saved\SaveGames\` on Windows). These files contain:

- Player progression data (current in-game year, completed events, inventory);
- World state snapshot (NPC relationship values, political map state);
- Chronicle log (in-game historical records the player has discovered).

Local save files never leave your device except when uploaded to Steam Cloud (see Section 4).

### 2.3 Gameplay Telemetry (Disabled by Default)

The Game contains an optional simulation tracing system controlled by the console variable `mahlanya.EnableSimulationTracing`. This variable defaults to `0` (disabled). When enabled by the user, it records anonymized gameplay event data (such as which historical events were triggered, which simulation branches were explored, and performance frame timing) to a local log file. This data is not transmitted to the Developer unless you explicitly choose to submit a bug report and attach the log file manually. We do not have any automated telemetry pipeline that sends data to our servers.

## 3. Data We Do NOT Collect

We want to be explicit about what we do not collect:

- **No personal names** — we do not collect your real name, username, or Steam display name;
- **No email addresses** — we do not have access to the email address associated with your Steam account;
- **No precise location data** — we do not collect GPS location, IP-derived precise location, or home address. Steam may record the country associated with your account for its own purposes, which is governed by Valve's Privacy Policy, not this one;
- **No payment information** — all purchases are processed by Valve through Steam; we receive no payment card data;
- **No microphone or camera data** — the Game does not access audio input devices or cameras;
- **No browsing history or cross-app tracking**.

## 4. Steam Cloud

If you have Steam Cloud enabled for Mahlanya in your Steam client settings, save game data (the files described in Section 2.2) will be synchronized to Valve's cloud infrastructure. This data is stored and governed by Valve Corporation's Steam Privacy Policy, available at https://store.steampowered.com/privacy_agreement/. The Developer does not have independent access to your Steam Cloud data; we can only read save data returned to the local device by the Steam API during an active game session.

Steam Cloud synchronizes the following save slots:
- `MahlanyaPlayerProfile` — character and progression data
- `MahlanyaWorldState` — world simulation state
- `MahlanyaChronicle` — discovered historical records

## 5. Crash Reports

If the Game crashes, the Unreal Engine 5 crash reporter may activate. The crash reporter collects an anonymous crash dump containing the call stack at the time of the crash, the Game version number, and basic hardware configuration (CPU, GPU, available RAM, operating system version). This dump is sent to Epic Games' crash reporting servers and is governed by Epic Games' Privacy Policy, available at https://www.epicgames.com/privacypolicy. The Developer may access aggregated, anonymized crash data through Epic's developer dashboard to identify and fix bugs. Crash dumps do not contain your Steam Account ID, save game data, or any personal information.

You may disable the UE5 crash reporter by launching the Game with the `-NoExceptionHandler` command-line flag.

## 6. Multiplayer

When you join or host a co-operative multiplayer session, your Steam Account ID is shared with other players in the session via Steam Networking. This is necessary for session establishment and is a standard feature of Steam multiplayer. No additional personal data is transmitted to other players.

## 7. Children

The Game is rated for Teen (T) audiences by the ESRB and is not directed at users under the age of 13. We do not knowingly collect personal information from children under 13. If you are a parent or guardian and believe your child has used the Game in a way that resulted in the collection of personal information, please contact us at charleskris9@gmail.com.

## 8. Data Retention

Local save files persist until you delete them or uninstall the Game. Steam Cloud copies persist according to Steam's cloud storage policies. The Developer does not maintain any server-side database of user-specific data that would need to be retained or deleted on request.

## 9. Your Rights

Depending on your jurisdiction, you may have rights regarding your personal data including the right to access, correct, or delete data held about you. Because the Developer does not maintain a server-side user database, most data subject requests should be directed to Valve (for Steam account data) or Epic Games (for crash report data). For questions about locally stored save files, you may simply delete them from your device. If you have a request the Developer can assist with, contact charleskris9@gmail.com.

## 10. Changes to This Policy

We may update this Privacy Policy from time to time. When we do, we will update the Effective Date at the top of this document and post the revised policy in the Game's Steam store page and in the `docs/legal/` directory of the game installation. Continued use of the Game after a revised policy is posted constitutes acceptance of the updated policy.

## 11. Contact

For privacy-related questions or concerns, contact:

**Charles Bartaria**
Email: charleskris9@gmail.com
