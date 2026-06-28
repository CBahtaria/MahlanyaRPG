"""Inter-settlement political graph. Hierarchy: Umuti→Indvuna→Inkhundla→Lusengo. Plan 4."""
import json
import math
import pathlib


def _euclidean_km(a, b):
    """Euclidean distance in km between two UTM coordinate pairs [x, y]."""
    dx = a[0] - b[0]
    dy = a[1] - b[1]
    return math.sqrt(dx * dx + dy * dy) / 1000.0


def build_political_graph(
    settlements_dir: pathlib.Path,
    dem_path: pathlib.Path,
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Build an inter-settlement political graph from settlement JSON manifests.

    Hierarchy levels:
        0 = Umuti (homestead)
        1 = Indvuna village
        2 = Inkhundla
        3 = Lusengo (royal kraal)

    Parameters
    ----------
    settlements_dir:
        Directory containing one JSON manifest per settlement.
    dem_path:
        Path to the DEM raster (currently unused; reserved for future
        elevation-aware distance calculation).
    output_path:
        Directory to write ``political_graph.ndjson`` and
        ``trade_routes.geojson`` into.

    Returns
    -------
    pathlib.Path
        Path to ``output_path / "political_graph.ndjson"``.
    """
    output_path = pathlib.Path(output_path)
    output_path.mkdir(parents=True, exist_ok=True)

    ndjson_path = output_path / "political_graph.ndjson"
    geojson_path = output_path / "trade_routes.geojson"

    # Load all settlement manifests ----------------------------------------
    settlements_dir = pathlib.Path(settlements_dir)
    settlements = []
    if settlements_dir.exists():
        for json_file in sorted(settlements_dir.glob("*.json")):
            with open(json_file, encoding="utf-8") as fh:
                settlements.append(json.load(fh))

    # Group by hierarchy level
    by_level = {}
    for s in settlements:
        lvl = s["hierarchy_level"]
        by_level.setdefault(lvl, []).append(s)

    edges = []

    # Hierarchy edges: each settlement → nearest settlement one level higher --
    for lvl in (0, 1, 2):
        upper_lvl = lvl + 1
        if lvl not in by_level or upper_lvl not in by_level:
            continue
        uppers = by_level[upper_lvl]
        for s in by_level[lvl]:
            loc = s["location_utm"]
            nearest = min(uppers, key=lambda u: _euclidean_km(loc, u["location_utm"]))
            dist = _euclidean_km(loc, nearest["location_utm"])
            edges.append({
                "from": s["settlement_id"],
                "to": nearest["settlement_id"],
                "relation": "vassal",
                "cattle_tribute": int(s["cattle_count"] * 0.1),
                "distance_km": round(dist, 6),
            })

    # Alliance edges: same level, within 15 km --------------------------------
    for lvl, group in by_level.items():
        for i in range(len(group)):
            for j in range(i + 1, len(group)):
                a = group[i]
                b = group[j]
                dist = _euclidean_km(a["location_utm"], b["location_utm"])
                if dist <= 15.0:
                    edges.append({
                        "from": a["settlement_id"],
                        "to": b["settlement_id"],
                        "relation": "ally",
                        "cattle_tribute": 0,
                        "distance_km": round(dist, 6),
                    })

    # Feud edges: different level, within 5 km, both level 0, cattle diff > 20
    level0 = by_level.get(0, [])
    for i in range(len(level0)):
        for j in range(i + 1, len(level0)):
            # Already handled by ally check above (same level); feuds require
            # different hierarchy levels per spec — but spec says level 0
            # settlements within 5 km with cattle_count difference > 20.
            # Since they are the same level (0), we only create a feud edge
            # when the cattle difference exceeds 20 AND they are within 5 km.
            # (The spec says "different hierarchy level at odds" but then
            # restricts to level 0, so we treat it as: same level 0 pair,
            # close range, large cattle disparity → feud overrides ally.)
            pass

    # Per spec: "within 5km of different hierarchy level at odds
    # (level 0, cattle_count difference > 20)".  Interpreted as:
    # any two level-0 settlements within 5 km whose cattle counts differ by > 20
    # get a feud edge (in addition to or instead of ally).
    for i in range(len(level0)):
        for j in range(i + 1, len(level0)):
            a = level0[i]
            b = level0[j]
            dist = _euclidean_km(a["location_utm"], b["location_utm"])
            if dist <= 5.0 and abs(a["cattle_count"] - b["cattle_count"]) > 20:
                edges.append({
                    "from": a["settlement_id"],
                    "to": b["settlement_id"],
                    "relation": "feud",
                    "cattle_tribute": 0,
                    "distance_km": round(dist, 6),
                })

    # Build lookup for coordinates (for GeoJSON)
    coord_lookup = {s["settlement_id"]: s["location_utm"] for s in settlements}

    # Write NDJSON ------------------------------------------------------------
    with open(ndjson_path, "w", encoding="utf-8") as fh:
        for edge in edges:
            fh.write(json.dumps(edge) + "\n")

    # Write GeoJSON -----------------------------------------------------------
    features = []
    for edge in edges:
        from_coord = coord_lookup.get(edge["from"])
        to_coord = coord_lookup.get(edge["to"])
        if from_coord is None or to_coord is None:
            continue
        feature = {
            "type": "Feature",
            "geometry": {
                "type": "LineString",
                "coordinates": [
                    [from_coord[0], from_coord[1]],
                    [to_coord[0], to_coord[1]],
                ],
            },
            "properties": {
                "from": edge["from"],
                "to": edge["to"],
                "relation": edge["relation"],
            },
        }
        features.append(feature)

    geojson = {
        "type": "FeatureCollection",
        "features": features,
    }
    with open(geojson_path, "w", encoding="utf-8") as fh:
        json.dump(geojson, fh)

    return ndjson_path
