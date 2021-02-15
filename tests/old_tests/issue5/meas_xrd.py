import os.path

import numpy as np
import pandas.io.parsers


class MeasXRD:
    def __init__(self, path: str):
        if not os.path.isfile(path):
            raise FileNotFoundError("Invalid XRD file path:", path)

        row_ind = 2
        self.params = {}
        with open(path, "r") as file:
            line = file.readline()
            if line != "[Measurement conditions]\n":
                raise ValueError("XRD measurement file does not contain a valid header")

            line = file.readline()
            while line not in ["[Scan points]\n", ""]:
                row_ind += 1
                columns = line.rstrip("\n").split(",", 1)
                self.params[columns[0]] = columns[1]
                line = file.readline()

        self.data = pandas.io.parsers.read_csv(
            path, skiprows=row_ind, dtype={"Angle": np.float_, "Intensity": np.int_}, engine="c"
        )
