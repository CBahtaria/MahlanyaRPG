# VR Setup — Mahlanya World Viewer

Target platforms: Meta Quest 3 (OpenXR), SteamVR.

Build: enable OpenXR plugin in .uproject (done). Package with `-platform=Win64 -config=Development`.

Proximity Parks visible in VR: AProximityPark actors spawned at real GPS coordinates.
GPS → UE5 world coords: origin Manzini (-26.32, 31.14); 1 degree latitude ≈ 111,000 cm north; 1 degree longitude ≈ 97,000 cm east at Swazi latitude.
