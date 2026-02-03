#!/usr/bin/env python3
"""
CLoTH Lightning Network Simulator Test Runner

Usage:
    python run_tests.py                    # Run all tests
    python run_tests.py --test test_name   # Run specific test
    python run_tests.py --routing cloth    # Run tests for specific routing
    python run_tests.py --verbose          # Verbose output
    python run_tests.py --list             # List all tests
"""

import argparse
import csv
import json
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Dict, Optional, Any


# Constants
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
OUTPUT_DIR = SCRIPT_DIR / "output"


@dataclass
class TestCase:
    """Definition of a single test case."""
    name: str
    network_dir: str
    payments_file: str
    expected_file: str
    config_overrides: Dict[str, str] = field(default_factory=dict)
    seed: int = 42
    description: str = ""


@dataclass
class TestResult:
    """Result of a single test execution."""
    name: str
    passed: bool
    message: str
    details: Optional[Dict[str, Any]] = None


class TestRunner:
    """Runs CLoTH simulation tests and validates results."""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.results: List[TestResult] = []

    def log(self, msg: str):
        if self.verbose:
            print(f"  [DEBUG] {msg}")

    def build_config_args(self, test: TestCase) -> str:
        """Build config override arguments for run-simulation.sh."""
        # Base config pointing to test files (relative paths work via rsync copy)
        network_path = f"tests/networks/{test.network_dir}"
        config = {
            "generate_network_from_file": "true",
            "nodes_filename": f"{network_path}/nodes.csv",
            "channels_filename": f"{network_path}/channels.csv",
            "edges_filename": f"{network_path}/edges.csv",
            "generate_payments_from_file": "true",
            "payments_filename": f"tests/payments/{test.payments_file}",
            "payment_timeout": "60000",
            "average_payment_forward_interval": "100",
            "variance_payment_forward_interval": "1",
            "faulty_node_probability": "0.0",
        }
        config.update(test.config_overrides)
        return " ".join(f"{k}={v}" for k, v in config.items())

    def run_simulation(self, test: TestCase, output_dir: Path) -> bool:
        """Run simulation via run-simulation.sh."""
        args = self.build_config_args(test)
        cmd = f"./run-simulation.sh {test.seed} {output_dir} {args}"

        self.log(f"Command: {cmd[:100]}...")

        try:
            result = subprocess.run(
                cmd, shell=True, cwd=str(PROJECT_ROOT),
                capture_output=True, text=True, timeout=300
            )
            if result.returncode != 0:
                self.log(f"Failed: {result.stderr[:500]}")
                return False
            return True
        except subprocess.TimeoutExpired:
            self.log("Timeout after 5 minutes")
            return False
        except Exception as e:
            self.log(f"Error: {e}")
            return False

    def load_payments_output(self, output_dir: Path) -> List[Dict]:
        """Load payments_output.csv."""
        csv_path = output_dir / "payments_output.csv"
        if not csv_path.exists():
            return []
        with open(csv_path, 'r') as f:
            return list(csv.DictReader(f))

    def load_edges_output(self, output_dir: Path) -> List[Dict]:
        """Load edges_output.csv."""
        csv_path = output_dir / "edges_output.csv"
        if not csv_path.exists():
            return []
        with open(csv_path, 'r') as f:
            return list(csv.DictReader(f))

    def load_groups_output(self, output_dir: Path) -> List[Dict]:
        """Load groups_output.csv."""
        csv_path = output_dir / "groups_output.csv"
        if not csv_path.exists():
            return []
        with open(csv_path, 'r') as f:
            return list(csv.DictReader(f))

    def load_expected(self, test: TestCase) -> Dict:
        """Load expected results JSON."""
        path = SCRIPT_DIR / "expected" / test.expected_file
        with open(path, 'r') as f:
            return json.load(f)

    def check_assertion(self, actual: Dict, assertion: Dict) -> tuple[bool, str]:
        """Check a single assertion."""
        field_name = assertion["field"]
        op = assertion["op"]
        expected = assertion.get("value")
        actual_val = actual.get(field_name, "")

        try:
            if op == "eq":
                return str(actual_val) == str(expected), f"{field_name}={actual_val}"
            elif op == "neq":
                return str(actual_val) != str(expected), f"{field_name}={actual_val}"
            elif op == "gt":
                return int(actual_val) > int(expected), f"{field_name}={actual_val}"
            elif op == "gte":
                return int(actual_val) >= int(expected), f"{field_name}={actual_val}"
            elif op == "lt":
                return int(actual_val) < int(expected), f"{field_name}={actual_val}"
            elif op == "lte":
                return int(actual_val) <= int(expected), f"{field_name}={actual_val}"
            elif op == "not_empty":
                ok = actual_val not in ("", None, "-1")
                return ok, f"{field_name}={'not empty' if ok else 'empty'}"
            elif op == "empty":
                ok = actual_val in ("", None, "-1")
                return ok, f"{field_name}={'empty' if ok else 'not empty'}"
        except (ValueError, TypeError) as e:
            return False, f"Error: {e}"

        return False, f"Unknown op: {op}"

    def validate_results(self, test: TestCase, output_dir: Path) -> TestResult:
        """Validate results against expected."""
        expected = self.load_expected(test)
        payments = self.load_payments_output(output_dir)
        edges = self.load_edges_output(output_dir)
        groups = self.load_groups_output(output_dir)

        if not payments:
            return TestResult(test.name, False, "No output found")

        failures = []
        
        # Validate payments
        for exp in expected.get("payments", []):
            actual = next((p for p in payments if int(p["id"]) == exp["id"]), None)
            if not actual:
                failures.append(f"Payment {exp['id']} not found")
                continue
            for assertion in exp.get("assertions", []):
                ok, msg = self.check_assertion(actual, assertion)
                if not ok:
                    failures.append(f"Payment {exp['id']}: {msg} (expected {assertion['op']} {assertion.get('value', '')})")

        # Check shards if needed
        if expected.get("check_shards"):
            shards = [p for p in payments if int(p.get("is_shard", 0)) >= 1]
            if not shards:
                failures.append("Expected shards but none found")
            for shard in shards:
                for assertion in expected.get("shard_assertions", []):
                    ok, msg = self.check_assertion(shard, assertion)
                    if not ok:
                        failures.append(f"Shard {shard['id']}: {msg}")

        # Validate edges
        for exp_edge in expected.get("edges", []):
            actual = next((e for e in edges if int(e["id"]) == exp_edge["id"]), None)
            if not actual:
                failures.append(f"Edge {exp_edge['id']} not found")
                continue
            for assertion in exp_edge.get("assertions", []):
                ok, msg = self.check_assertion(actual, assertion)
                if not ok:
                    failures.append(f"Edge {exp_edge['id']}: {msg} (expected {assertion['op']} {assertion.get('value', '')})")

        # Validate groups
        for exp_group in expected.get("groups", []):
            actual = next((g for g in groups if int(g["id"]) == exp_group["id"]), None)
            if not actual:
                failures.append(f"Group {exp_group['id']} not found")
                continue
            for assertion in exp_group.get("assertions", []):
                ok, msg = self.check_assertion(actual, assertion)
                if not ok:
                    failures.append(f"Group {exp_group['id']}: {msg} (expected {assertion['op']} {assertion.get('value', '')})")

        if failures:
            return TestResult(test.name, False, "; ".join(failures[:3]))
        return TestResult(test.name, True, "All assertions passed")

    def run_test(self, test: TestCase) -> TestResult:
        """Run a single test."""
        print(f"\n{'='*60}")
        print(f"Test: {test.name}")
        if test.description:
            print(f"  {test.description}")
        print(f"{'='*60}")

        output_dir = OUTPUT_DIR / test.name
        output_dir.mkdir(parents=True, exist_ok=True)

        if not self.run_simulation(test, output_dir):
            return TestResult(test.name, False, "Simulation failed")

        result = self.validate_results(test, output_dir)
        status = "PASS" if result.passed else "FAIL"
        print(f"Result: [{status}] {result.message}")
        return result

    def run_all_tests(self, tests: List[TestCase]) -> int:
        """Run all tests."""
        for test in tests:
            self.results.append(self.run_test(test))

        # Summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)

        passed = sum(1 for r in self.results if r.passed)
        for r in self.results:
            status = "PASS" if r.passed else "FAIL"
            print(f"  [{status}] {r.name}")
            if not r.passed:
                print(f"         -> {r.message}")

        print(f"\nTotal: {len(self.results)} | Passed: {passed} | Failed: {len(self.results) - passed}")
        return 0 if passed == len(self.results) else 1


def get_test_cases() -> List[TestCase]:
    """Define all test cases."""
    return [
        # ============================================================
        # Basic routing tests - ALL METHODS
        # ============================================================
        
        # CLoTH Original - Basic tests
        TestCase("test_success_single_cloth", "simple_linear", "test_success_single.csv",
                 "test_success_single.json", {"routing_method": "cloth_original", "mpp": "0"},
                 description="[cloth_original] Single payment succeeds"),
        TestCase("test_retry_success_cloth", "imbalanced_4node", "test_retry_success.csv",
                 "test_retry_success.json", {"routing_method": "cloth_original", "mpp": "0"},
                 description="[cloth_original] Capacity failure -> retry -> success"),
        TestCase("test_capacity_exhaust_cloth", "imbalanced_4node", "test_capacity_exhaust.csv",
                 "test_capacity_exhaust.json", {"routing_method": "cloth_original", "mpp": "0"},
                 description="[cloth_original] All paths exhausted -> failure"),
        TestCase("test_no_path_cloth", "isolated_node", "test_no_path.csv",
                 "test_no_path.json", {"routing_method": "cloth_original", "mpp": "0"},
                 description="[cloth_original] No route exists -> failure"),

        # Ideal routing - Basic tests
        TestCase("test_success_single_ideal", "simple_linear", "test_success_single.csv",
                 "test_success_single.json", {"routing_method": "ideal", "mpp": "0"},
                 description="[ideal] Single payment succeeds"),
        TestCase("test_retry_success_ideal", "imbalanced_4node", "test_retry_success.csv",
                 "test_retry_success_ideal.json", {"routing_method": "ideal", "mpp": "0"},
                 description="[ideal] Capacity failure -> retry -> success"),
        TestCase("test_capacity_exhaust_ideal", "imbalanced_4node", "test_capacity_exhaust.csv",
                 "test_capacity_exhaust.json", {"routing_method": "ideal", "mpp": "0"},
                 description="[ideal] All paths exhausted -> failure"),
        TestCase("test_no_path_ideal", "isolated_node", "test_no_path.csv",
                 "test_no_path.json", {"routing_method": "ideal", "mpp": "0"},
                 description="[ideal] No route exists -> failure"),

        # Group routing (GCB) - Basic tests on grouped network
        TestCase("test_success_single_gcb", "grouped_network", "test_success_single_gcb.csv",
                 "test_success_single_gcb.json",
                 {"routing_method": "group_routing", "mpp": "0", "group_size": "5", "group_limit_rate": "0.3"},
                 description="[group_routing] Single payment succeeds on grouped network"),
        TestCase("test_retry_success_gcb", "grouped_network", "test_retry_success_gcb.csv",
                 "test_retry_success_gcb.json",
                 {"routing_method": "group_routing", "mpp": "0", "group_size": "5", "group_limit_rate": "0.3"},
                 description="[group_routing] Retry on non-group link failure"),
        TestCase("test_capacity_exhaust_gcb", "grouped_network", "test_capacity_exhaust_gcb.csv",
                 "test_capacity_exhaust_gcb.json",
                 {"routing_method": "group_routing", "mpp": "0", "group_size": "5", "group_limit_rate": "0.3"},
                 description="[group_routing] All paths exhausted -> failure"),
        TestCase("test_no_path_gcb", "isolated_node", "test_no_path.csv",
                 "test_no_path.json",
                 {"routing_method": "group_routing", "mpp": "0", "group_size": "5", "group_limit_rate": "0.3"},
                 description="[group_routing] No route exists -> failure"),

        # Group routing CUL - Basic tests on grouped network
        TestCase("test_success_single_cul", "grouped_network", "test_success_single_cul.csv",
                 "test_success_single_cul.json",
                 {"routing_method": "group_routing_cul", "mpp": "0", "group_size": "5"},
                 description="[group_routing_cul] Single payment succeeds on grouped network"),
        TestCase("test_retry_success_cul", "grouped_network", "test_retry_success_cul.csv",
                 "test_retry_success_cul.json",
                 {"routing_method": "group_routing_cul", "mpp": "0", "group_size": "5"},
                 description="[group_routing_cul] Retry on non-group link failure"),
        TestCase("test_capacity_exhaust_cul", "grouped_network", "test_capacity_exhaust_cul.csv",
                 "test_capacity_exhaust_cul.json",
                 {"routing_method": "group_routing_cul", "mpp": "0", "group_size": "5"},
                 description="[group_routing_cul] All paths exhausted -> failure"),
        TestCase("test_no_path_cul", "isolated_node", "test_no_path.csv",
                 "test_no_path.json",
                 {"routing_method": "group_routing_cul", "mpp": "0", "group_size": "5"},
                 description="[group_routing_cul] No route exists -> failure"),

        # ============================================================
        # MPP tests (Non-GCB)
        # ============================================================
        TestCase("test_mpp_single_path_cloth", "diamond_4node", "test_mpp_single_path.csv",
                 "test_mpp_single_path.json", {"routing_method": "cloth_original", "mpp": "1"},
                 description="[cloth_original+MPP] Amount fits single path"),
        TestCase("test_mpp_split_success_cloth", "diamond_4node", "test_mpp_split_success.csv",
                 "test_mpp_split_success_cloth.json",
                 {"routing_method": "cloth_original", "mpp": "1", "max_shard_count": "16"},
                 description="[cloth_original+MPP] Binary split succeeds"),
        TestCase("test_mpp_max_shards_cloth", "parallel_6node", "test_mpp_max_shards.csv",
                 "test_mpp_max_shards.json",
                 {"routing_method": "cloth_original", "mpp": "1", "max_shard_count": "4"},
                 description="[cloth_original+MPP] Max shards -> failure"),

        # MPP tests (GCB)
        TestCase("test_mpp_single_path_gcb", "diamond_4node", "test_mpp_single_path.csv",
                 "test_mpp_single_path.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3"},
                 description="[group_routing+MPP] Amount fits single path"),
        TestCase("test_mpp_split_success_gcb", "diamond_4node", "test_mpp_split_success.csv",
                 "test_mpp_split_success_gcb.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3"},
                 description="[group_routing+MPP] Optimal split succeeds"),
        TestCase("test_mpp_max_shards_gcb", "parallel_6node", "test_mpp_max_shards.csv",
                 "test_mpp_max_shards.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "max_shard_count": "4"},
                 description="[group_routing+MPP] Max shards -> failure"),

        # ============================================================
        # GCB/CUL specific parameter tests
        # ============================================================
        TestCase("test_gcb_group_limit", "grouped_network", "test_gcb_group_limit.csv",
                 "test_gcb_group_limit.json",
                 {"routing_method": "group_routing", "mpp": "0", "group_size": "5", "group_limit_rate": "0.1"},
                 description="[group_routing] Group capacity affects routing"),
        TestCase("test_gcb_cul_threshold", "grouped_network", "test_gcb_cul_closure.csv",
                 "test_gcb_cul_closure.json",
                 {"routing_method": "group_routing_cul", "mpp": "0", "group_size": "5"},
                 description="[group_routing_cul] CUL threshold behavior"),

        # GCB MPP comprehensive tests
        # 1. Small amount - no split needed (single path sufficient with group capacity)
        TestCase("test_mpp_gcb_small_amount", "diamond_4node", "test_mpp_gcb_small_amount.csv",
                 "test_mpp_gcb_small_amount.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3"},
                 description="[group_routing+MPP] Small amount - no split needed"),

        # 2. Multi-path split using group capacity information
        TestCase("test_mpp_gcb_multipath_split", "diamond_4node", "test_mpp_gcb_multipath_split.csv",
                 "test_mpp_gcb_multipath_split.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "8"},
                 description="[group_routing+MPP] Optimal multi-path split using group capacity"),

        # 3. Shard re-split on failure
        TestCase("test_mpp_gcb_shard_resplit", "diamond_4node", "test_mpp_gcb_shard_resplit.csv",
                 "test_mpp_gcb_shard_resplit.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "8"},
                 description="[group_routing+MPP] Failed shard triggers re-split"),

        # 4. No path available even with splitting (amount too large)
        TestCase("test_mpp_gcb_no_path", "diamond_4node", "test_mpp_gcb_no_path.csv",
                 "test_mpp_gcb_no_path.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "16"},
                 description="[group_routing+MPP] Amount too large - all paths exhausted"),

        # 5. Timeout during shard processing
        TestCase("test_mpp_gcb_timeout", "diamond_4node", "test_mpp_gcb_timeout.csv",
                 "test_mpp_gcb_timeout.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "payment_timeout": "1"},
                 description="[group_routing+MPP] Payment times out during shard processing"),

        # 6. Rollback mechanism on partial success
        TestCase("test_mpp_gcb_rollback", "diamond_4node", "test_mpp_gcb_rollback.csv",
                 "test_mpp_gcb_rollback.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "8"},
                 description="[group_routing+MPP] Partial success triggers rollback"),

        # 7. Group limit affects MPP path selection
        TestCase("test_mpp_gcb_group_limit_rate", "diamond_4node", "test_mpp_gcb_group_limit.csv",
                 "test_mpp_gcb_group_limit.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.1"},
                 description="[group_routing+MPP] Tight group_limit_rate affects splitting"),

        # 8. Max shards boundary test (using multipath network)
        TestCase("test_mpp_gcb_max_shards_boundary", "multipath_8node", "test_mpp_gcb_max_shards_boundary.csv",
                 "test_mpp_gcb_max_shards_boundary.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "8"},
                 description="[group_routing+MPP] Payment needs exactly max shards"),

        # 3-way split test (requires 3+ parallel paths)
        TestCase("test_mpp_gcb_3way_split", "multipath_8node", "test_mpp_gcb_3way_split.csv",
                 "test_mpp_gcb_3way_split.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "8"},
                 description="[group_routing+MPP] 3-way split across parallel paths"),

        # 4-way split test (requires 4 parallel paths)
        TestCase("test_mpp_gcb_4way_split", "multipath_8node", "test_mpp_gcb_4way_split.csv",
                 "test_mpp_gcb_4way_split.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "8"},
                 description="[group_routing+MPP] 4-way split using all available routes"),

        # Many shards test
        TestCase("test_mpp_gcb_many_shards", "multipath_8node", "test_mpp_gcb_many_shards.csv",
                 "test_mpp_gcb_many_shards.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "16"},
                 description="[group_routing+MPP] Multiple shards across multiple paths"),

        # 9. CUL + MPP combination
        TestCase("test_mpp_cul_split", "diamond_4node", "test_mpp_cul_split.csv",
                 "test_mpp_cul_split.json",
                 {"routing_method": "group_routing_cul", "mpp": "1", "group_size": "5", "max_shard_count": "8"},
                 description="[group_routing_cul+MPP] CUL threshold-based split"),

        # 10. Large group size affects MPP behavior
        TestCase("test_mpp_gcb_large_group", "diamond_4node", "test_mpp_split_success.csv",
                 "test_mpp_split_success_gcb.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "10", "group_limit_rate": "0.5"},
                 description="[group_routing+MPP] Large group size affects capacity estimation"),

        # 11. Very restrictive group limit rate - may cause different split behavior
        TestCase("test_mpp_gcb_restrictive_limit", "diamond_4node", "test_mpp_split_success.csv",
                 "test_mpp_split_success_gcb.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.05", "max_shard_count": "16"},
                 description="[group_routing+MPP] Very restrictive group_limit_rate"),

        # 12. MPP with max_shard_count=2 (minimal splitting)
        TestCase("test_mpp_gcb_minimal_shards", "diamond_4node", "test_mpp_split_success.csv",
                 "test_mpp_split_success_gcb.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "2"},
                 description="[group_routing+MPP] Minimal shard count (2)"),

        # 13. CUL with different threshold values
        TestCase("test_mpp_cul_high_threshold", "diamond_4node", "test_mpp_split_success.csv",
                 "test_mpp_split_success_gcb.json",
                 {"routing_method": "group_routing_cul", "mpp": "1", "group_size": "5", "max_shard_count": "8"},
                 description="[group_routing_cul+MPP] High CUL threshold behavior"),

        # 14. GCB MPP failure then retry with smaller shards
        TestCase("test_mpp_gcb_retry_smaller", "parallel_6node", "test_mpp_max_shards.csv",
                 "test_mpp_max_shards.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3", "max_shard_count": "4"},
                 description="[group_routing+MPP] Max shards exceeded -> failure"),

        # 15. Single group member edge case
        TestCase("test_mpp_gcb_single_group_member", "simple_linear", "test_success_single.csv",
                 "test_success_single.json",
                 {"routing_method": "group_routing", "mpp": "1", "group_size": "5", "group_limit_rate": "0.3"},
                 description="[group_routing+MPP] Single path network - no MPP needed"),
    ]


def main():
    parser = argparse.ArgumentParser(description="CLoTH Test Runner")
    parser.add_argument("--test", "-t", help="Run specific test (partial match)")
    parser.add_argument("--routing", "-r", choices=["cloth", "group", "cul", "ideal"])
    parser.add_argument("--mpp", action="store_true", help="Run only MPP tests")
    parser.add_argument("--verbose", "-v", action="store_true")
    parser.add_argument("--list", "-l", action="store_true", help="List all tests")
    args = parser.parse_args()

    tests = get_test_cases()

    if args.list:
        print("Available tests:")
        for t in tests:
            print(f"  {t.name}: {t.description}")
        return 0

    if args.test:
        tests = [t for t in tests if args.test.lower() in t.name.lower()]
    if args.routing:
        method = {"cloth": "cloth_original", "group": "group_routing",
                  "cul": "group_routing_cul", "ideal": "ideal"}[args.routing]
        tests = [t for t in tests if t.config_overrides.get("routing_method") == method]
    if args.mpp:
        tests = [t for t in tests if "mpp" in t.name.lower()]

    if not tests:
        print("No tests matched")
        return 1

    print(f"Running {len(tests)} test(s)...")
    return TestRunner(verbose=args.verbose).run_all_tests(tests)


if __name__ == "__main__":
    sys.exit(main())
