"""Test the scientific normalization independently of benchmark output agreement."""
import importlib.util
from pathlib import Path
import unittest
import numpy as np
spec = importlib.util.spec_from_file_location('compare', Path(__file__).parents[1] / 'tools/reaper/compare.py')
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


class RoundoffPolicy(unittest.TestCase):
    def test_small_rate_not_hidden(self):
        a = np.array([1.e-30])
        b = a * (1 + 128 * np.finfo(float).eps)
        self.assertTrue(compare.compare_arrays(a, b, False)[1][0])
        self.assertFalse(compare.compare_arrays(a, b, True)[1][0])

    def test_boundary_and_ulp(self):
        a = np.array([1., -1., 1.e-30, 0.])
        b = np.nextafter(a, np.array([2., -2., 2.e-30, 0.]))
        values, failed = compare.compare_arrays(a, b, False)
        self.assertFalse(failed.any())
        self.assertEqual(values['ulp'].tolist(), [1, 1, 1, 0])

    def test_zero_sign_and_nonfinite(self):
        a = np.array([0., 0., 1., np.inf])
        b = np.array([-0., 1.e-300, np.nan, np.inf])
        with np.errstate(invalid='ignore'):
            _, failed = compare.compare_arrays(a, b, False)
        self.assertEqual(failed.tolist(), [False, True, True, True])


if __name__ == '__main__':
    unittest.main()
