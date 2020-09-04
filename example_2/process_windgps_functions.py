import numpy as np
import os
import matplotlib.pyplot as plt
import pandas as pd

import load_windgps_data_to_pandas

def fix_millis_time(df):
    idx = np.where( np.diff(df.millis.astype(int))<0 )[0]
    df.loc[idx+1, 'millis'] = np.nan
    df.millis.interpolate(inplace=True)
    df.millis = df.millis.astype(int)
    return df