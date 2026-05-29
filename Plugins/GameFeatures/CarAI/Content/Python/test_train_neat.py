#!/usr/bin/env python3
"""
Unit tests for train_neat.py (NEAT training for Racing AI).

Run from the same directory as train_neat.py:
    python -m unittest test_train_neat
    python -m unittest test_train_neat -v
Or:
    python test_train_neat.py
"""

import io
import json
import pickle
import sys
import tempfile
import unittest
from pathlib import Path

# Ensure we can import train_neat from the same directory
_script_dir = Path(__file__).resolve().parent
if str(_script_dir) not in sys.path:
    sys.path.insert(0, str(_script_dir))

import train_neat
import neat


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

def make_valid_contract(**overrides):
    """Return a contract dict valid for load_contract (all required keys, observation_size=15)."""
    base = {
        "fitness_dir": "/tmp/fitness",
        "genome_dir": "/tmp/genome",
        "checkpoint_dir": "/tmp/checkpoint",
        "best_genome_path": "/tmp/best_genome.json",
        "observation_size": 15,
        "action_size": 3,
        "population_size": 8,
    }
    base.update(overrides)
    return base


def write_manifest(path: Path, contract: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        json.dump(contract, f, indent=2)


# ---------------------------------------------------------------------------
# Config template and INI helpers
# ---------------------------------------------------------------------------

class TestConfigTemplate(unittest.TestCase):
    def test_template_contains_neat_section(self):
        out = train_neat._config_template(15, 3, 10)
        self.assertIn("[NEAT]", out)
        self.assertIn("no_fitness_termination", out)
        self.assertIn("pop_size              = 10", out)

    def test_template_contains_default_genome_section(self):
        out = train_neat._config_template(15, 3, 10)
        self.assertIn("[DefaultGenome]", out)
        self.assertIn("single_structural_mutation", out)
        self.assertIn("structural_mutation_surer", out)
        self.assertIn("bias_init_type", out)
        self.assertIn("num_inputs              = 15", out)
        self.assertIn("num_outputs             = 3", out)

    def test_template_pop_size_varies(self):
        out1 = train_neat._config_template(15, 3, 8)
        out2 = train_neat._config_template(15, 3, 20)
        self.assertIn("pop_size              = 8", out1)
        self.assertIn("pop_size              = 20", out2)


class TestParseIniSections(unittest.TestCase):
    def test_parses_multiple_sections(self):
        text = """
[NEAT]
a = 1
b = 2

[DefaultGenome]
x = 3
"""
        sections = train_neat._parse_ini_sections(text)
        self.assertIn("NEAT", sections)
        self.assertIn("DefaultGenome", sections)
        self.assertIn("a = 1", sections["NEAT"])
        self.assertIn("x = 3", sections["DefaultGenome"])

    def test_empty_text_returns_empty_dict(self):
        self.assertEqual(train_neat._parse_ini_sections(""), {})
        self.assertEqual(train_neat._parse_ini_sections("\n\n"), {})

    def test_section_body_excludes_next_header(self):
        text = "[A]\nbody1\n[B]\nbody2"
        sections = train_neat._parse_ini_sections(text)
        self.assertNotIn("body2", sections["A"])
        self.assertIn("body1", sections["A"])
        self.assertIn("body2", sections["B"])


class TestFindMissingFields(unittest.TestCase):
    def test_full_template_has_no_missing_fields(self):
        text = train_neat._config_template(15, 3, 8)
        missing = train_neat._find_missing_fields(text)
        self.assertEqual(missing, [], msg=f"Missing: {missing}")

    def test_missing_no_fitness_termination_detected(self):
        text = train_neat._config_template(15, 3, 8)
        text = text.replace("no_fitness_termination", "x_no_fitness_termination")
        missing = train_neat._find_missing_fields(text)
        self.assertTrue(any("no_fitness_termination" in m for m in missing))

    def test_missing_single_structural_mutation_detected(self):
        text = train_neat._config_template(15, 3, 8)
        text = text.replace("single_structural_mutation", "x_single_structural_mutation")
        missing = train_neat._find_missing_fields(text)
        self.assertTrue(any("single_structural_mutation" in m for m in missing))

    def test_missing_bias_init_type_detected(self):
        text = train_neat._config_template(15, 3, 8)
        text = text.replace("bias_init_type", "x_bias_init_type")
        missing = train_neat._find_missing_fields(text)
        self.assertTrue(any("bias_init_type" in m for m in missing))

    def test_missing_response_init_type_detected(self):
        text = train_neat._config_template(15, 3, 8)
        text = text.replace("response_init_type", "x_response_init_type")
        missing = train_neat._find_missing_fields(text)
        self.assertTrue(any("response_init_type" in m for m in missing))

    def test_missing_weight_init_type_detected(self):
        text = train_neat._config_template(15, 3, 8)
        text = text.replace("weight_init_type", "x_weight_init_type")
        missing = train_neat._find_missing_fields(text)
        self.assertTrue(any("weight_init_type" in m for m in missing))

    def test_missing_enabled_rate_to_true_add_detected(self):
        text = train_neat._config_template(15, 3, 8)
        text = text.replace("enabled_rate_to_true_add", "x_enabled_rate_to_true_add")
        missing = train_neat._find_missing_fields(text)
        self.assertTrue(any("enabled_rate_to_true_add" in m for m in missing))

    def test_missing_min_species_size_detected(self):
        text = train_neat._config_template(15, 3, 8)
        text = text.replace("min_species_size", "x_min_species_size")
        missing = train_neat._find_missing_fields(text)
        self.assertTrue(any("min_species_size" in m for m in missing))


class TestCreateOrValidateConfig(unittest.TestCase):
    def test_creates_config_when_file_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            self.assertFalse(config_path.exists())
            cap = io.StringIO()
            old_stdout = sys.stdout
            sys.stdout = cap
            try:
                train_neat.create_or_validate_config(
                    str(config_path), obs_size=15, action_size=3, population_size=8
                )
            finally:
                sys.stdout = old_stdout
            self.assertTrue(config_path.exists())
            content = config_path.read_text(encoding="utf-8")
            self.assertIn("no_fitness_termination", content)
            self.assertIn("pop_size              = 8", content)
            self.assertIn("CREATED", cap.getvalue())

    def test_regenerates_when_required_field_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            # Write minimal invalid config (missing no_fitness_termination etc.)
            config_path.write_text("[NEAT]\npop_size = 8\n\n[DefaultGenome]\nnum_inputs = 15\nnum_outputs = 3\n")
            cap = io.StringIO()
            old_stdout = sys.stdout
            sys.stdout = cap
            try:
                train_neat.create_or_validate_config(
                    str(config_path), obs_size=15, action_size=3, population_size=8
                )
            finally:
                sys.stdout = old_stdout
            content = config_path.read_text(encoding="utf-8")
            self.assertIn("no_fitness_termination", content, "Config should be regenerated with full template")
            self.assertIn("REGENERATED", cap.getvalue())

    def test_regenerates_when_pop_size_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 99))
            cap = io.StringIO()
            old_stdout = sys.stdout
            sys.stdout = cap
            try:
                train_neat.create_or_validate_config(
                    str(config_path), obs_size=15, action_size=3, population_size=8
                )
            finally:
                sys.stdout = old_stdout
            content = config_path.read_text(encoding="utf-8")
            self.assertIn("pop_size              = 8", content)
            self.assertIn("REGENERATED", cap.getvalue())

    def test_kept_when_complete_and_pop_matches(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 8))
            cap = io.StringIO()
            old_stdout = sys.stdout
            sys.stdout = cap
            try:
                train_neat.create_or_validate_config(
                    str(config_path), obs_size=15, action_size=3, population_size=8
                )
            finally:
                sys.stdout = old_stdout
            self.assertIn("KEPT", cap.getvalue())

    def test_stale_config_regenerated_then_neat_config_succeeds(self):
        """Regression: stale config missing required field is regenerated; neat.Config() must succeed after."""
        import re
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            broken = train_neat._config_template(15, 3, 8)
            broken = broken.replace("no_fitness_termination", "x_no_fitness_termination")
            config_path.write_text(broken)
            cap = io.StringIO()
            old_stdout = sys.stdout
            sys.stdout = cap
            try:
                train_neat.create_or_validate_config(
                    str(config_path), obs_size=15, action_size=3, population_size=8
                )
            finally:
                sys.stdout = old_stdout
            self.assertIn("REGENERATED", cap.getvalue(), "Stale config must be regenerated, not KEPT")
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            self.assertEqual(config.genome_config.num_inputs, 15)
            self.assertEqual(config.genome_config.num_outputs, 3)

    def test_manifest_population_size_written_to_config_file(self):
        """Regression: manifest population_size must control pop_size in neat_config on disk."""
        import re
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            train_neat.create_or_validate_config(
                str(config_path), obs_size=15, action_size=3, population_size=12
            )
            content = config_path.read_text()
            m = re.search(r"^\s*pop_size\s*=\s*(\d+)", content, re.MULTILINE)
            self.assertIsNotNone(m, "Config must contain pop_size line")
            self.assertEqual(int(m.group(1)), 12, "pop_size in file must equal manifest population_size")


# ---------------------------------------------------------------------------
# Checkpoint paths
# ---------------------------------------------------------------------------

class TestCheckpointPaths(unittest.TestCase):
    def test_get_checkpoint_path(self):
        p = train_neat.get_checkpoint_path(Path("/tmp/ck"))
        self.assertEqual(p, Path("/tmp/ck") / train_neat.CHECKPOINT_FILENAME)

    def test_find_checkpoint_returns_none_when_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(train_neat.find_checkpoint(Path(tmp)))

    def test_find_checkpoint_returns_path_when_exists(self):
        with tempfile.TemporaryDirectory() as tmp:
            ck = Path(tmp) / train_neat.CHECKPOINT_FILENAME
            ck.write_text("dummy")
            self.assertEqual(train_neat.find_checkpoint(Path(tmp)), ck)


# ---------------------------------------------------------------------------
# Contract loading
# ---------------------------------------------------------------------------

class TestLoadContract(unittest.TestCase):
    def test_valid_manifest_returns_contract(self):
        with tempfile.TemporaryDirectory() as tmp:
            manifest_path = Path(tmp) / "neat_contract.json"
            write_manifest(manifest_path, make_valid_contract())
            contract = train_neat.load_contract(str(manifest_path))
            self.assertEqual(contract["observation_size"], 15)
            self.assertEqual(contract["population_size"], 8)

    def test_missing_manifest_file_exits(self):
        with self.assertRaises(SystemExit) as cm:
            train_neat.load_contract("/nonexistent/neat_contract_12345.json")
        self.assertEqual(cm.exception.code, 1)

    def test_manifest_missing_required_key_exits(self):
        with tempfile.TemporaryDirectory() as tmp:
            manifest_path = Path(tmp) / "manifest.json"
            write_manifest(manifest_path, {"fitness_dir": "/x", "genome_dir": "/y"})
            with self.assertRaises(SystemExit) as cm:
                train_neat.load_contract(str(manifest_path))
            self.assertEqual(cm.exception.code, 1)

    def test_manifest_wrong_observation_size_exits(self):
        with tempfile.TemporaryDirectory() as tmp:
            manifest_path = Path(tmp) / "manifest.json"
            write_manifest(manifest_path, make_valid_contract(observation_size=10))
            with self.assertRaises(SystemExit) as cm:
                train_neat.load_contract(str(manifest_path))
            self.assertEqual(cm.exception.code, 1)

    def test_manifest_wrong_action_size_exits(self):
        with tempfile.TemporaryDirectory() as tmp:
            manifest_path = Path(tmp) / "manifest.json"
            write_manifest(manifest_path, make_valid_contract(action_size=2))
            with self.assertRaises(SystemExit) as cm:
                train_neat.load_contract(str(manifest_path))
            self.assertEqual(cm.exception.code, 1)

    def test_manifest_population_size_too_small_exits(self):
        with tempfile.TemporaryDirectory() as tmp:
            manifest_path = Path(tmp) / "manifest.json"
            write_manifest(manifest_path, make_valid_contract(population_size=1))
            with self.assertRaises(SystemExit) as cm:
                train_neat.load_contract(str(manifest_path))
            self.assertEqual(cm.exception.code, 1)


# ---------------------------------------------------------------------------
# FitnessLoader
# ---------------------------------------------------------------------------

class TestFitnessLoader(unittest.TestCase):
    def test_load_for_generation_missing_file_returns_empty(self):
        with tempfile.TemporaryDirectory() as tmp:
            loader = train_neat.FitnessLoader(tmp)
            self.assertEqual(loader.load_for_generation(0), {})

    def test_load_for_generation_parses_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "generation_0.json"
            path.write_text(json.dumps({
                "generation": 0,
                "genomes": [{"genome_id": 1, "fitness": 10.5}, {"genome_id": 2, "fitness": 20.0}],
            }))
            loader = train_neat.FitnessLoader(tmp)
            out = loader.load_for_generation(0)
            self.assertEqual(out, {1: 10.5, 2: 20.0})

    def test_load_latest_generation_empty_dir_returns_empty(self):
        with tempfile.TemporaryDirectory() as tmp:
            loader = train_neat.FitnessLoader(tmp)
            # Redirect stdout so Unicode print (e.g. warning symbol) does not fail on Windows cp1252
            old_stdout = sys.stdout
            sys.stdout = io.StringIO()
            try:
                self.assertEqual(loader.load_latest_generation(), {})
            finally:
                sys.stdout = old_stdout


class TestFitnessMapValidation(unittest.TestCase):
    def test_validation_accepts_exact_population_ids(self):
        train_neat.validate_fitness_map_for_population({1: 10.0, 2: 5.0}, [1, 2], 0)

    def test_validation_rejects_missing_ids(self):
        with self.assertRaises(ValueError):
            train_neat.validate_fitness_map_for_population({1: 10.0}, [1, 2], 0)

    def test_validation_rejects_extra_ids(self):
        with self.assertRaises(ValueError):
            train_neat.validate_fitness_map_for_population({1: 10.0, 2: 5.0, 99: 1.0}, [1, 2], 0)


# ---------------------------------------------------------------------------
# Training state path
# ---------------------------------------------------------------------------

class TestTrainingStatePath(unittest.TestCase):
    def test_get_training_state_path(self):
        p = train_neat.get_training_state_path(Path("/tmp/genome"))
        self.assertEqual(p, Path("/tmp/genome") / train_neat.TRAINING_STATE_FILENAME)

    def test_write_training_state(self):
        with tempfile.TemporaryDirectory() as tmp:
            genome_dir = Path(tmp)
            train_neat.write_training_state(genome_dir, 5)
            state_path = train_neat.get_training_state_path(genome_dir)
            self.assertTrue(state_path.exists())
            data = json.loads(state_path.read_text())
            self.assertEqual(data["exported_generation"], 5)


# ---------------------------------------------------------------------------
# Checkpoint save/load (requires valid neat config)
# ---------------------------------------------------------------------------

class TestCheckpointSaveLoad(unittest.TestCase):
    def test_save_and_load_checkpoint_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 4))
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            population = neat.Population(config)
            checkpoint_path = Path(tmp) / train_neat.CHECKPOINT_FILENAME
            train_neat.save_checkpoint(checkpoint_path, population, config, 0)
            self.assertTrue(checkpoint_path.exists())
            pop2, config2, last = train_neat.load_checkpoint(checkpoint_path, 15, 3)
            self.assertEqual(last, 0)
            self.assertEqual(config2.genome_config.num_inputs, 15)
            self.assertEqual(config2.genome_config.num_outputs, 3)
            self.assertEqual(len(pop2.population), 4)

    def test_load_checkpoint_wrong_format_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad_path = Path(tmp) / "bad.pkl"
            with open(bad_path, "wb") as f:
                pickle.dump((1, 2), f)  # only 2 items
            with self.assertRaises(ValueError):
                train_neat.load_checkpoint(bad_path, 15, 3)

    def test_load_checkpoint_config_mismatch_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            # Config with 10 inputs, 2 outputs
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(10, 2, 4))
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            population = neat.Population(config)
            checkpoint_path = Path(tmp) / train_neat.CHECKPOINT_FILENAME
            train_neat.save_checkpoint(checkpoint_path, population, config, 0)
            with self.assertRaises(ValueError):
                train_neat.load_checkpoint(checkpoint_path, 15, 3)  # contract expects 15, 3


# ---------------------------------------------------------------------------
# Genome export contract for Unreal (NEATGenomeImporter)
# ---------------------------------------------------------------------------

def _validate_exported_genome_for_unreal(data: dict) -> list:
    """
    Validate exported genome JSON against what Unreal NEATGenomeImporter expects.
    Returns list of error messages (empty if valid).
    """
    errors = []

    # Required top-level fields and no null for numbers
    for key in ("genome_id", "generation", "num_inputs", "num_outputs"):
        if key not in data:
            errors.append(f"Missing required field: {key}")
        elif data[key] is None:
            errors.append(f"Required field must not be null: {key}")

    if data.get("fitness") is None and "fitness" in data:
        errors.append("fitness must not be null if present")

    nodes = data.get("nodes")
    if nodes is None:
        errors.append("Missing 'nodes' array")
        return errors

    node_ids = set()
    for i, n in enumerate(nodes):
        if not isinstance(n, dict):
            errors.append(f"Invalid node entry at index {i}")
            continue
        nid = n.get("id")
        if nid is None:
            errors.append(f"Node at index {i} missing 'id'")
        else:
            node_ids.add(nid)
        if n.get("bias") is None and "bias" in n:
            errors.append(f"Node {nid}: bias must not be null")
        if n.get("response") is None and "response" in n:
            errors.append(f"Node {nid}: response must not be null")

    conns = data.get("connections")
    if conns is None:
        errors.append("Missing 'connections' array")
        return errors

    for i, c in enumerate(conns):
        if not isinstance(c, dict):
            errors.append(f"Invalid connection entry at index {i}")
            continue
        in_node = c.get("in_node")
        out_node = c.get("out_node")
        if in_node is None:
            errors.append(f"Connection {i}: in_node must not be null")
        elif in_node not in node_ids:
            errors.append(f"Connection references missing in_node {in_node} (not in nodes array)")
        if out_node is None:
            errors.append(f"Connection {i}: out_node must not be null")
        elif out_node not in node_ids:
            errors.append(f"Connection references missing out_node {out_node} (not in nodes array)")
        if c.get("weight") is None and "weight" in c:
            errors.append(f"Connection {i}: weight must not be null")

    return errors


class TestGenomeExportUnrealContract(unittest.TestCase):
    """
    Export must satisfy Unreal NEATGenomeImporter contract:
    - Every connection's in_node and out_node must exist in the nodes array (by id).
    - No required numeric fields may be null.
    """

    def test_exported_genome_all_connection_nodes_present_in_nodes_array(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 4))
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            population = neat.Population(config)
            genome = next(iter(population.population.values()))
            out_path = Path(tmp) / "genome.json"
            train_neat.GenomeExporter.export_genome(
                genome, genome.key, 0, 0.0, str(out_path), config, verbose=False
            )
            data = json.loads(out_path.read_text())
            errs = _validate_exported_genome_for_unreal(data)
            self.assertEqual(errs, [], f"Export does not satisfy Unreal contract: {errs}")

    def test_exported_genome_no_null_numeric_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 4))
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            population = neat.Population(config)
            genome = next(iter(population.population.values()))
            out_path = Path(tmp) / "genome.json"
            train_neat.GenomeExporter.export_genome(
                genome, genome.key, 0, 0.0, str(out_path), config, verbose=False
            )
            data = json.loads(out_path.read_text())
            self.assertIsNotNone(data.get("genome_id"), "genome_id must not be null")
            self.assertIsNotNone(data.get("generation"), "generation must not be null")
            self.assertIsNotNone(data.get("num_inputs"), "num_inputs must not be null")
            self.assertIsNotNone(data.get("num_outputs"), "num_outputs must not be null")
            for node in data.get("nodes", []):
                self.assertIsNotNone(node.get("id"), f"node id must not be null: {node}")
                self.assertIsNotNone(node.get("bias"), f"node bias must not be null: {node}")
                self.assertIsNotNone(node.get("response"), f"node response must not be null: {node}")
            for conn in data.get("connections", []):
                self.assertIsNotNone(conn.get("in_node"), f"in_node must not be null: {conn}")
                self.assertIsNotNone(conn.get("out_node"), f"out_node must not be null: {conn}")
                self.assertIsNotNone(conn.get("weight"), f"weight must not be null: {conn}")

    def test_exported_genome_input_node_ids_are_negative_observation_indices(self):
        """Regression: Unreal observation[i] maps to node ID -(i+1); inputs must be -1..-num_inputs."""
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 4))
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            population = neat.Population(config)
            genome = next(iter(population.population.values()))
            out_path = Path(tmp) / "genome.json"
            train_neat.GenomeExporter.export_genome(
                genome, genome.key, 0, 0.0, str(out_path), config, verbose=False
            )
            data = json.loads(out_path.read_text())
            num_inputs = data["num_inputs"]
            node_ids = {n["id"] for n in data["nodes"]}
            expected_input_ids = {-(i + 1) for i in range(num_inputs)}
            missing = expected_input_ids - node_ids
            self.assertEqual(missing, set(), f"Exported genome must include input node IDs {expected_input_ids}; missing {missing}")


# ---------------------------------------------------------------------------
# GenomeExporter (needs a real genome from neat)
# ---------------------------------------------------------------------------

class TestGenomeExporter(unittest.TestCase):
    def test_export_genome_produces_valid_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 4))
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            # Genome must come from a Population so innovation_tracker is set
            population = neat.Population(config)
            genome = next(iter(population.population.values()))
            genome_id = genome.key
            out_path = Path(tmp) / "genome_1.json"
            train_neat.GenomeExporter.export_genome(
                genome, genome_id, 0, 0.0, str(out_path), config, verbose=False
            )
            self.assertTrue(out_path.exists())
            data = json.loads(out_path.read_text())
            self.assertEqual(data["genome_id"], genome_id)
            self.assertEqual(data["generation"], 0)
            self.assertEqual(data["num_inputs"], 15)
            self.assertEqual(data["num_outputs"], 3)
            self.assertIsInstance(data["nodes"], list)
            self.assertIsInstance(data["connections"], list)


# ---------------------------------------------------------------------------
# Export population for Unreal
# ---------------------------------------------------------------------------

class TestExportPopulationForUnreal(unittest.TestCase):
    def test_export_population_creates_list_and_genome_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 2))
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            population = neat.Population(config)
            out_dir = Path(tmp) / "genomes"
            old_stdout = sys.stdout
            sys.stdout = io.StringIO()
            try:
                train_neat.export_population_for_unreal(population, config, out_dir, 0)
            finally:
                sys.stdout = old_stdout
            list_file = out_dir / "generation_0_genomes.json"
            self.assertTrue(list_file.exists())
            data = json.loads(list_file.read_text())
            self.assertEqual(data["generation"], 0)
            self.assertEqual(data["population_size"], 2)
            self.assertEqual(len(data["genomes"]), 2)
            for g in data["genomes"]:
                gid = g["genome_id"]
                self.assertTrue((out_dir / f"genome_{gid}.json").exists())


# ---------------------------------------------------------------------------
# Main entry (fresh start path)
# ---------------------------------------------------------------------------

class TestMainFreshStart(unittest.TestCase):
    """Run main() with a valid manifest and empty dirs to hit the fresh-start path."""

    def test_main_fresh_start_succeeds(self):
        with tempfile.TemporaryDirectory() as tmp:
            fitness_dir = Path(tmp) / "fitness"
            genome_dir = Path(tmp) / "genome"
            checkpoint_dir = Path(tmp) / "checkpoint"
            fitness_dir.mkdir()
            genome_dir.mkdir()
            checkpoint_dir.mkdir()
            manifest_path = Path(tmp) / "neat_contract.json"
            write_manifest(manifest_path, make_valid_contract(
                fitness_dir=str(fitness_dir),
                genome_dir=str(genome_dir),
                checkpoint_dir=str(checkpoint_dir),
                best_genome_path=str(Path(tmp) / "best_genome.json"),
            ))
            # Run from a copy of script dir so config is written in tmp, not in Content/Python
            config_in_tmp = Path(tmp) / "neat_config.txt"
            # We need main() to use our manifest and have config written under tmp.
            # train_neat.main() uses script_dir = Path(__file__).resolve().parent and
            # config_path = script_dir / CONFIG_FILE, so it always writes to Content/Python.
            # So we cannot redirect config to tmp without changing train_neat. Instead,
            # run main and let it write config next to train_neat.py; we'll use a dedicated
            # temp dir for fitness/genome/checkpoint and manifest. The config will be
            # created in Content/Python - that's acceptable for one test.
            # Alternatively: run the script as subprocess with env that points to a dir
            # that contains both manifest and a writable neat_config.txt. So: create
            # tmp, put manifest there, put a symlink or copy of train_neat? No.
            # Simpler: just run main() and pass manifest. The only side effect in the
            # repo is neat_config.txt in Content/Python - and we already have that file.
            # So main() will do: load contract, create_or_validate_config(script_dir/config),
            # neat.Config(script_dir/config), then create population, export to genome_dir (tmp),
            # save checkpoint to checkpoint_dir (tmp), write_training_state to genome_dir.
            # So the only thing that gets written outside tmp is the config file in
            # script_dir. To avoid touching script_dir, we'd need to monkey-patch
            # __file__ or the config path in train_neat. That's fragile.
            # Better: run main and accept that config might be updated in script dir.
            # Or run in subprocess and pass manifest; then the process will write
            # config to its script dir (Content/Python). So we run:
            #   python train_neat.py --manifest <tmp>/neat_contract.json
            # That will write to tmp for genome/fitness/checkpoint and to Content/Python
            # for config. So we don't need to change anything; we just run main().
            orig_argv = sys.argv
            sys.stdout = io.StringIO()
            sys.stderr = io.StringIO()
            sys.argv = ["train_neat.py", "--manifest", str(manifest_path)]
            try:
                train_neat.main()
            except SystemExit as e:
                sys.stdout = sys.__stdout__
                sys.stderr = sys.__stderr__
                self.fail(f"main() exited with code {e.code}")
            finally:
                sys.stdout = sys.__stdout__
                sys.stderr = sys.__stderr__
                sys.argv = orig_argv
            # Check outputs
            self.assertTrue((genome_dir / "generation_0_genomes.json").exists())
            self.assertTrue((checkpoint_dir / train_neat.CHECKPOINT_FILENAME).exists())
            self.assertTrue((genome_dir / train_neat.TRAINING_STATE_FILENAME).exists())
            state = json.loads((genome_dir / train_neat.TRAINING_STATE_FILENAME).read_text())
            self.assertEqual(state["exported_generation"], 0)


# ---------------------------------------------------------------------------
# ASCII/cp1252-safe output (no UnicodeEncodeError on Windows when Unreal captures stdout)
# ---------------------------------------------------------------------------

class TestAsciiSafeOutput(unittest.TestCase):
    """
    Ensure all user-facing print() in train_neat use only cp1252-encodable characters.
    When Python is launched by Unreal on Windows, stdout may use cp1252; emojis then
    cause UnicodeEncodeError and break the training handoff.
    """

    def _stdout_cp1252_strict(self):
        """Return a stream that encodes with cp1252 and strict (raises on non-encodable)."""
        return io.TextIOWrapper(io.BytesIO(), encoding="cp1252", errors="strict", line_buffering=True)

    def test_export_population_for_unreal_prints_cp1252_safe(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 2))
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            population = neat.Population(config)
            out_dir = Path(tmp) / "genomes"
            old_stdout = sys.stdout
            sys.stdout = self._stdout_cp1252_strict()
            try:
                train_neat.export_population_for_unreal(population, config, out_dir, 0)
            except UnicodeEncodeError as e:
                self.fail(f"export_population_for_unreal printed non-cp1252 character: {e}")
            finally:
                sys.stdout = old_stdout

    def test_fitness_loader_load_latest_empty_dir_prints_cp1252_safe(self):
        with tempfile.TemporaryDirectory() as tmp:
            loader = train_neat.FitnessLoader(tmp)
            old_stdout = sys.stdout
            sys.stdout = self._stdout_cp1252_strict()
            try:
                loader.load_latest_generation()
            except UnicodeEncodeError as e:
                self.fail(f"FitnessLoader.load_latest_generation printed non-cp1252 character: {e}")
            finally:
                sys.stdout = old_stdout

    def test_genome_exporter_verbose_prints_cp1252_safe(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = Path(tmp) / "neat_config.txt"
            config_path.write_text(train_neat._config_template(15, 3, 2))
            config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                str(config_path),
            )
            population = neat.Population(config)
            genome = next(iter(population.population.values()))
            genome_id = genome.key
            out_path = Path(tmp) / "genome_1.json"
            old_stdout = sys.stdout
            sys.stdout = self._stdout_cp1252_strict()
            try:
                train_neat.GenomeExporter.export_genome(
                    genome, genome_id, 0, 0.0, str(out_path), config, verbose=True
                )
            except UnicodeEncodeError as e:
                self.fail(f"GenomeExporter.export_genome(verbose=True) printed non-cp1252 character: {e}")
            finally:
                sys.stdout = old_stdout

    def test_fitness_loader_load_latest_with_file_prints_cp1252_safe(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "generation_0.json").write_text(
                json.dumps({"generation": 0, "genomes": [{"genome_id": 0, "fitness": 1.0}]})
            )
            loader = train_neat.FitnessLoader(tmp)
            old_stdout = sys.stdout
            sys.stdout = self._stdout_cp1252_strict()
            try:
                loader.load_latest_generation()
            except UnicodeEncodeError as e:
                self.fail(f"FitnessLoader.load_latest_generation (with file) printed non-cp1252 character: {e}")
            finally:
                sys.stdout = old_stdout


# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    unittest.main(verbosity=2)
