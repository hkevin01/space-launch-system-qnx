#!/usr/bin/env python3
"""
Test runner for QNX Space Launch System GUI

This script provides various options for running the GUI test suite:
- Basic test run
- Coverage analysis
- Performance benchmarks
- Specific test categories
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(
        description="Run QNX Space Launch System GUI tests",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python run_tests.py                    # Run all tests
  python run_tests.py --coverage         # Run with coverage
  python run_tests.py --benchmark        # Run performance tests
  python run_tests.py --unit-only        # Run only unit tests
  python run_tests.py --integration      # Run integration tests
  python run_tests.py --stress           # Run stress tests
  python run_tests.py --verbose          # Verbose output
  python run_tests.py --html-report      # Generate HTML report
        """
    )
    
    parser.add_argument(
        '--coverage',
        action='store_true',
        help='Run tests with coverage analysis'
    )
    
    parser.add_argument(
        '--benchmark',
        action='store_true',
        help='Run performance benchmark tests'
    )
    
    parser.add_argument(
        '--unit-only',
        action='store_true',
        help='Run only unit tests'
    )
    
    parser.add_argument(
        '--integration',
        action='store_true',
        help='Run integration tests'
    )
    
    parser.add_argument(
        '--stress',
        action='store_true',
        help='Run stress and performance tests'
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose test output'
    )
    
    parser.add_argument(
        '--html-report',
        action='store_true',
        help='Generate HTML test report'
    )
    
    parser.add_argument(
        '--parallel', '-j',
        type=int,
        metavar='N',
        help='Run tests in parallel with N workers'
    )
    
    parser.add_argument(
        '--filter', '-k',
        type=str,
        metavar='PATTERN',
        help='Only run tests matching the pattern'
    )
    
    parser.add_argument(
        '--install-deps',
        action='store_true',
        help='Install test dependencies before running'
    )
    
    args = parser.parse_args()
    
    # Change to GUI directory
    gui_dir = Path(__file__).parent
    os.chdir(gui_dir)
    
    # Install dependencies if requested
    if args.install_deps:
        print("Installing test dependencies...")
        deps = [
            'pytest', 'pytest-qt', 'pytest-mock', 'pytest-cov',
            'pytest-html', 'pytest-xdist', 'pytest-benchmark'
        ]
        subprocess.run([sys.executable, '-m', 'pip', 'install'] + deps)
        print("Dependencies installed.\n")
    
    # Build pytest command
    cmd = [sys.executable, '-m', 'pytest', 'test_gui.py']
    
    # Add verbosity
    if args.verbose:
        cmd.append('-v')
    else:
        cmd.append('-q')
    
    # Add coverage if requested
    if args.coverage:
        cmd.extend([
            '--cov=enhanced_launch_gui',
            '--cov=launch_gui',
            '--cov-report=html:test_results/coverage',
            '--cov-report=term',
            '--cov-report=xml:test_results/coverage.xml'
        ])
    
    # Add HTML report if requested
    if args.html_report or args.coverage:
        cmd.extend([
            '--html=test_results/test_report.html',
            '--self-contained-html'
        ])
    
    # Add parallel execution if requested
    if args.parallel:
        cmd.extend(['-n', str(args.parallel)])
    
    # Add pattern filter if provided
    if args.filter:
        cmd.extend(['-k', args.filter])
    
    # Select test categories
    test_markers = []
    
    if args.unit_only:
        test_markers.append('not integration and not stress and not benchmark')
    elif args.integration:
        test_markers.append('integration')
    elif args.stress:
        test_markers.append('stress or performance')
    elif args.benchmark:
        test_markers.append('benchmark')
    
    if test_markers:
        cmd.extend(['-m', ' or '.join(test_markers)])
    
    # Create test results directory
    (gui_dir / 'test_results').mkdir(exist_ok=True)
    
    # Run the tests
    print("Running QNX Space Launch System GUI tests...")
    print(f"Command: {' '.join(cmd[2:])}")  # Don't show python path
    print("-" * 60)
    
    try:
        result = subprocess.run(cmd, check=False)
        
        print("-" * 60)
        
        # Show results location
        if args.html_report or args.coverage:
            report_file = gui_dir / 'test_results' / 'test_report.html'
            if report_file.exists():
                print(f"HTML report: {report_file}")
        
        if args.coverage:
            coverage_dir = gui_dir / 'test_results' / 'coverage'
            if coverage_dir.exists():
                print(f"Coverage report: {coverage_dir / 'index.html'}")
        
        return result.returncode
        
    except KeyboardInterrupt:
        print("\nTests interrupted by user")
        return 1
    except Exception as e:
        print(f"Error running tests: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
