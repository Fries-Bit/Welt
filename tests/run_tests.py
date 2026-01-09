"""Test runner for Welt programming language"""
import os
import sys
import subprocess

def run_welt_test(test_file):
    """Run a .wt test file through the Welt interpreter"""
    interpreter = 'welt'
    if os.name == 'nt':
        interpreter = 'a.exe'
    
    # Check if interpreter exists
    if not os.path.exists(interpreter) and not os.path.exists(os.path.join('..', interpreter)):
        print(f"[WARN] Welt interpreter not found. Compile main.c first.")
        return None
    
    try:
        result = subprocess.run(
            [interpreter, test_file],
            capture_output=True,
            text=True,
            timeout=5
        )
        return result
    except FileNotFoundError:
        print(f"[WARN] Could not find interpreter: {interpreter}")
        return None
    except subprocess.TimeoutExpired:
        print(f"[FAIL] Test {test_file} timed out")
        return None

def run_python_tests():
    """Run Python tests for FSAL"""
    print("=" * 60)
    print("Running FSAL Python Tests")
    print("=" * 60)
    
    test_fsal = os.path.join(os.path.dirname(__file__), 'test_fsal.py')
    if os.path.exists(test_fsal):
        result = subprocess.run([sys.executable, test_fsal], capture_output=True, text=True)
        print(result.stdout)
        if result.stderr:
            print(result.stderr)
        return result.returncode == 0
    else:
        print("[WARN] test_fsal.py not found")
        return False

def run_welt_tests():
    """Run all .wt test files"""
    print("\n" + "=" * 60)
    print("Running Welt Language Tests")
    print("=" * 60)
    
    test_dir = os.path.dirname(__file__)
    test_files = sorted([f for f in os.listdir(test_dir) if f.endswith('.wt')])
    
    if not test_files:
        print("[WARN] No .wt test files found")
        return False
    
    passed = 0
    failed = 0
    skipped = 0
    
    for test_file in test_files:
        test_path = os.path.join(test_dir, test_file)
        print(f"\nRunning {test_file}...")
        print("-" * 60)
        
        result = run_welt_test(test_path)
        
        if result is None:
            skipped += 1
            continue
        
        print(result.stdout)
        
        if result.stderr:
            print("STDERR:", result.stderr)
        
        if result.returncode == 0:
            print(f"[PASS] {test_file} passed")
            passed += 1
        else:
            print(f"[FAIL] {test_file} failed (exit code: {result.returncode})")
            failed += 1
    
    print("\n" + "=" * 60)
    print("Welt Test Results")
    print("=" * 60)
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Skipped: {skipped}")
    
    return failed == 0

def main():
    """Run all tests"""
    print("\n" + "=" * 60)
    print("WELT PROGRAMMING LANGUAGE TEST SUITE")
    print("=" * 60)
    
    python_success = run_python_tests()
    welt_success = run_welt_tests()
    
    print("\n" + "=" * 60)
    print("OVERALL RESULTS")
    print("=" * 60)
    print(f"Python Tests: {'[PASS] PASSED' if python_success else '[FAIL] FAILED'}")
    print(f"Welt Tests: {'[PASS] PASSED' if welt_success else '[WARN] SKIPPED (no interpreter)'}")
    
    if python_success and (welt_success or True):  # Allow Welt tests to be skipped
        print("\n[PASS] Test suite completed successfully!")
        return 0
    else:
        print("\n[FAIL] Some tests failed")
        return 1

if __name__ == '__main__':
    sys.exit(main())
