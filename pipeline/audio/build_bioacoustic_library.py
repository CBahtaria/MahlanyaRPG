"""28 Eswatini bird species + insect/frog chorus bioacoustic library. Plan 6."""
import json
import pathlib

# 28 bird species endemic or common in Eswatini, assigned to altitude bands.
# freq_hz: (low, high) call frequency range
# active_windows: list of (start_hour, end_hour) when species calls
# biome_zones: from terrain pipeline output
_BIRD_SPECIES = [
    # Highveld species (> 1400m)
    {"id": "SWN001", "name": "Gurney's Sugarbird",     "freq_hz": (2800, 4200),
     "biome_zones": ["afromontane_forest"],              "active_windows": [(5, 9), (16, 19)]},
    {"id": "SWN002", "name": "Cape Vulture",             "freq_hz": (200, 600),
     "biome_zones": ["afromontane_forest", "highveld_grassland"],
     "active_windows": [(7, 17)]},
    {"id": "SWN003", "name": "Bald Ibis",               "freq_hz": (400, 900),
     "biome_zones": ["highveld_grassland"],              "active_windows": [(5, 8), (17, 19)]},
    {"id": "SWN004", "name": "Buff-streaked Chat",       "freq_hz": (3000, 5500),
     "biome_zones": ["highveld_grassland"],              "active_windows": [(5, 10)]},
    {"id": "SWN005", "name": "Ground Woodpecker",        "freq_hz": (1500, 3000),
     "biome_zones": ["highveld_grassland", "afromontane_forest"],
     "active_windows": [(6, 11), (14, 18)]},
    # Middleveld / Swazi thornveld species (600–1400m)
    {"id": "SWN006", "name": "Hadeda Ibis",             "freq_hz": (500, 1200),
     "biome_zones": ["swazi_thornveld", "riparian_forest"],
     "active_windows": [(5, 7), (17, 19)]},
    {"id": "SWN007", "name": "Lilac-breasted Roller",   "freq_hz": (800, 2000),
     "biome_zones": ["swazi_thornveld"],                 "active_windows": [(6, 12)]},
    {"id": "SWN008", "name": "African Fish Eagle",       "freq_hz": (600, 1500),
     "biome_zones": ["riparian_forest"],                 "active_windows": [(5, 9), (15, 18)]},
    {"id": "SWN009", "name": "Woodland Kingfisher",      "freq_hz": (1800, 3500),
     "biome_zones": ["swazi_thornveld", "riparian_forest"],
     "active_windows": [(5, 11), (15, 19)]},
    {"id": "SWN010", "name": "Grey Go-away-bird",        "freq_hz": (600, 1200),
     "biome_zones": ["swazi_thornveld"],                 "active_windows": [(6, 11), (14, 18)]},
    {"id": "SWN011", "name": "Purple-crested Turaco",    "freq_hz": (500, 1000),
     "biome_zones": ["swazi_thornveld", "afromontane_forest"],
     "active_windows": [(5, 10), (16, 19)]},
    {"id": "SWN012", "name": "Red-faced Cisticola",      "freq_hz": (3000, 6000),
     "biome_zones": ["swazi_thornveld"],                 "active_windows": [(5, 10)]},
    {"id": "SWN013", "name": "Southern Yellow-billed Hornbill", "freq_hz": (700, 1800),
     "biome_zones": ["swazi_thornveld"],                 "active_windows": [(6, 12), (14, 17)]},
    {"id": "SWN014", "name": "Black-bellied Bustard",    "freq_hz": (400, 800),
     "biome_zones": ["swazi_thornveld", "acacia_savanna"],
     "active_windows": [(5, 8)]},
    # Lowveld / acacia savanna species (< 600m)
    {"id": "SWN015", "name": "Southern Carmine Bee-eater", "freq_hz": (2000, 4000),
     "biome_zones": ["acacia_savanna"],                  "active_windows": [(6, 11), (14, 18)]},
    {"id": "SWN016", "name": "Racket-tailed Roller",    "freq_hz": (1000, 2500),
     "biome_zones": ["acacia_savanna"],                  "active_windows": [(5, 10)]},
    {"id": "SWN017", "name": "Crested Barbet",           "freq_hz": (1200, 2500),
     "biome_zones": ["acacia_savanna", "swazi_thornveld"],
     "active_windows": [(5, 18)]},
    {"id": "SWN018", "name": "African Hoopoe",           "freq_hz": (600, 1200),
     "biome_zones": ["acacia_savanna", "swazi_thornveld"],
     "active_windows": [(5, 11), (15, 18)]},
    {"id": "SWN019", "name": "Sabota Lark",              "freq_hz": (2500, 5000),
     "biome_zones": ["acacia_savanna"],                  "active_windows": [(5, 11)]},
    {"id": "SWN020", "name": "White-fronted Bee-eater",  "freq_hz": (1500, 3500),
     "biome_zones": ["riparian_forest", "acacia_savanna"],
     "active_windows": [(6, 12), (14, 18)]},
    # Riparian / wetland species
    {"id": "SWN021", "name": "Malachite Kingfisher",    "freq_hz": (5000, 8000),
     "biome_zones": ["riparian_forest"],                 "active_windows": [(5, 18)]},
    {"id": "SWN022", "name": "Giant Kingfisher",         "freq_hz": (1500, 3000),
     "biome_zones": ["riparian_forest"],                 "active_windows": [(5, 18)]},
    {"id": "SWN023", "name": "African Darter",           "freq_hz": (200, 500),
     "biome_zones": ["riparian_forest"],                 "active_windows": [(6, 11)]},
    {"id": "SWN024", "name": "Hamerkop",                 "freq_hz": (400, 900),
     "biome_zones": ["riparian_forest"],                 "active_windows": [(5, 19)]},
    # Nocturnal species
    {"id": "SWN025", "name": "African Wood Owl",         "freq_hz": (400, 900),
     "biome_zones": ["afromontane_forest", "swazi_thornveld"],
     "active_windows": [(19, 24), (0, 5)]},
    {"id": "SWN026", "name": "Fiery-necked Nightjar",   "freq_hz": (1200, 2500),
     "biome_zones": ["swazi_thornveld"],                 "active_windows": [(18, 24), (0, 6)]},
    {"id": "SWN027", "name": "Spotted Eagle-Owl",        "freq_hz": (300, 800),
     "biome_zones": ["swazi_thornveld", "acacia_savanna", "highveld_grassland"],
     "active_windows": [(18, 24), (0, 6)]},
    {"id": "SWN028", "name": "African Scops-Owl",        "freq_hz": (800, 1500),
     "biome_zones": ["swazi_thornveld"],                 "active_windows": [(19, 24), (0, 5)]},
]

# Insect and frog chorus (seasonal)
_INVERTEBRATE_CHORUS = [
    {
        "id": "INV001",
        "name": "Summer Cicada",
        "type": "insect",
        "freq_hz": (2500, 4000),
        "active_months": [10, 11, 12, 1, 2, 3],  # Oct–Mar (wet season)
        "active_windows": [(9, 18)],
        "biome_zones": ["swazi_thornveld", "acacia_savanna"],
    },
    {
        "id": "INV002",
        "name": "Rain Frog Chorus",
        "type": "frog",
        "freq_hz": (800, 1500),
        "active_months": [10, 11, 12, 1, 2, 3],
        "active_windows": [(17, 24), (0, 6)],  # Dusk through dawn after rain
        "trigger": "precipitation_gt_5mm",
        "biome_zones": ["riparian_forest", "acacia_savanna", "swazi_thornveld"],
    },
    {
        "id": "INV003",
        "name": "Bush Cricket",
        "type": "insect",
        "freq_hz": (4000, 8000),
        "active_months": [1, 2, 3, 10, 11, 12],
        "active_windows": [(19, 24), (0, 5)],
        "biome_zones": ["swazi_thornveld", "highveld_grassland"],
    },
]


def build_bioacoustic_library(output_path: pathlib.Path) -> pathlib.Path:
    """Assemble and write the Eswatini bioacoustic library to output_path.

    Output JSON structure:
        {
          "bird_species": [...],           28 species
          "invertebrate_chorus": [...],    3 chorus types
          "biome_zones": [str, ...],       5 biome names
          "active_time_note": str
        }
    """
    output_path = pathlib.Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    biome_zones = [
        "afromontane_forest",
        "swazi_thornveld",
        "acacia_savanna",
        "riparian_forest",
        "highveld_grassland",
    ]

    data = {
        "bird_species": _BIRD_SPECIES,
        "invertebrate_chorus": _INVERTEBRATE_CHORUS,
        "biome_zones": biome_zones,
        "active_time_note": (
            "active_windows are (start_hour, end_hour) in 24h local time. "
            "Windows crossing midnight use end_hour < start_hour convention."
        ),
    }

    with open(output_path, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=2)

    return output_path
