import json
import pathlib
import pytest
from pipeline.settlements.build_political_graph import build_political_graph

def make_settlement(sid, x, y, level, wives=2, sons=1, cattle=20):
    return {"settlement_id": sid, "location_utm": [x, y],
            "num_wives": wives, "num_sons": sons,
            "cattle_count": cattle, "hierarchy_level": level}

def write_settlements(tmp_path, settlements):
    sdir = tmp_path / "settlements"
    sdir.mkdir()
    for s in settlements:
        (sdir / f"{s['settlement_id']}.json").write_text(json.dumps(s))
    return sdir

class TestBuildPoliticalGraph:
    def test_empty_settlements_dir_returns_path(self, tmp_path):
        """Empty settlements dir → writes empty files, returns ndjson path."""
        sdir = tmp_path / "empty_settlements"
        sdir.mkdir()
        out = build_political_graph(sdir, tmp_path / "dem.tif", tmp_path / "out")
        assert out.name == "political_graph.ndjson"
        assert out.exists()
        assert out.read_text() == ""

    def test_missing_settlements_dir_returns_path(self, tmp_path):
        """Non-existent settlements dir → graceful empty output."""
        out = build_political_graph(
            tmp_path / "nonexistent", tmp_path / "dem.tif", tmp_path / "out2")
        assert out.name == "political_graph.ndjson"

    def test_vassal_edge_created(self, tmp_path):
        """Umuti (level 0) → nearest Indvuna (level 1) gets vassal edge."""
        settlements = [
            make_settlement("S001", 0, 0, 0, cattle=20),
            make_settlement("S002", 5000, 0, 1, cattle=50),
        ]
        sdir = write_settlements(tmp_path, settlements)
        out = build_political_graph(sdir, tmp_path / "dem.tif", tmp_path / "out")
        lines = [l for l in out.read_text().strip().split("\n") if l]
        edges = [json.loads(l) for l in lines]
        vassal_edges = [e for e in edges if e.get("relation") == "vassal"]
        assert len(vassal_edges) >= 1
        assert vassal_edges[0]["from"] == "S001"
        assert vassal_edges[0]["to"] == "S002"

    def test_vassal_cattle_tribute_10_percent(self, tmp_path):
        """Vassal tribute = floor(cattle_count * 0.1)."""
        settlements = [
            make_settlement("S001", 0, 0, 0, cattle=45),
            make_settlement("S002", 1000, 0, 1, cattle=100),
        ]
        sdir = write_settlements(tmp_path, settlements)
        out = build_political_graph(sdir, tmp_path / "dem.tif", tmp_path / "out")
        edges = [json.loads(l) for l in out.read_text().strip().split("\n") if l]
        vassal_edges = [e for e in edges if e["from"] == "S001" and e["relation"] == "vassal"]
        assert len(vassal_edges) == 1
        assert vassal_edges[0]["cattle_tribute"] == 4  # floor(45 * 0.1)

    def test_distance_km_correct(self, tmp_path):
        """Distance in km = Euclidean UTM distance / 1000."""
        settlements = [
            make_settlement("S001", 0, 0, 0),
            make_settlement("S002", 10000, 0, 1),  # exactly 10km
        ]
        sdir = write_settlements(tmp_path, settlements)
        out = build_political_graph(sdir, tmp_path / "dem.tif", tmp_path / "out")
        edges = [json.loads(l) for l in out.read_text().strip().split("\n") if l]
        vassal_edges = [e for e in edges if e.get("relation") == "vassal"]
        assert abs(vassal_edges[0]["distance_km"] - 10.0) < 0.01

    def test_ally_edge_same_level_within_15km(self, tmp_path):
        """Two settlements of same level within 15km get ally edge."""
        settlements = [
            make_settlement("A1", 0, 0, 0),
            make_settlement("A2", 10000, 0, 0),  # 10km
        ]
        sdir = write_settlements(tmp_path, settlements)
        out = build_political_graph(sdir, tmp_path / "dem.tif", tmp_path / "out")
        edges = [json.loads(l) for l in out.read_text().strip().split("\n") if l]
        ally_edges = [e for e in edges if e.get("relation") == "ally"]
        assert len(ally_edges) >= 1

    def test_no_ally_edge_beyond_15km(self, tmp_path):
        """Two settlements 20km apart do NOT get ally edge."""
        settlements = [
            make_settlement("A1", 0, 0, 0),
            make_settlement("A2", 20000, 0, 0),  # 20km
        ]
        sdir = write_settlements(tmp_path, settlements)
        out = build_political_graph(sdir, tmp_path / "dem.tif", tmp_path / "out")
        edges = [json.loads(l) for l in out.read_text().strip().split("\n") if l]
        ally_edges = [e for e in edges if e.get("relation") == "ally"]
        assert len(ally_edges) == 0

    def test_geojson_written(self, tmp_path):
        """trade_routes.geojson is valid GeoJSON FeatureCollection."""
        settlements = [
            make_settlement("S001", 0, 0, 0, cattle=20),
            make_settlement("S002", 5000, 0, 1, cattle=50),
        ]
        sdir = write_settlements(tmp_path, settlements)
        out = build_political_graph(sdir, tmp_path / "dem.tif", tmp_path / "out")
        geojson_path = out.parent / "trade_routes.geojson"
        assert geojson_path.exists()
        data = json.loads(geojson_path.read_text())
        assert data["type"] == "FeatureCollection"
        assert isinstance(data["features"], list)

    def test_geojson_linestring_geometry(self, tmp_path):
        """Each GeoJSON feature has LineString geometry with 2 coordinates."""
        settlements = [
            make_settlement("S001", 0, 0, 0, cattle=20),
            make_settlement("S002", 5000, 0, 1, cattle=50),
        ]
        sdir = write_settlements(tmp_path, settlements)
        out = build_political_graph(sdir, tmp_path / "dem.tif", tmp_path / "out")
        geojson_path = out.parent / "trade_routes.geojson"
        data = json.loads(geojson_path.read_text())
        for feat in data["features"]:
            assert feat["geometry"]["type"] == "LineString"
            coords = feat["geometry"]["coordinates"]
            assert len(coords) == 2
            for coord in coords:
                assert len(coord) == 2  # [easting, northing]

    def test_output_path_created(self, tmp_path):
        """Output directory is created if it doesn't exist."""
        settlements = [make_settlement("S001", 0, 0, 0)]
        sdir = write_settlements(tmp_path, settlements)
        nested_out = tmp_path / "a" / "b" / "c"
        out = build_political_graph(sdir, tmp_path / "dem.tif", nested_out)
        assert nested_out.exists()

    def test_single_settlement_no_edges(self, tmp_path):
        """Single settlement with no neighbours → zero edges."""
        settlements = [make_settlement("SOLO", 0, 0, 0)]
        sdir = write_settlements(tmp_path, settlements)
        out = build_political_graph(sdir, tmp_path / "dem.tif", tmp_path / "out")
        content = out.read_text().strip()
        # May be empty or have zero lines
        edges = [json.loads(l) for l in content.split("\n") if l]
        assert len(edges) == 0
