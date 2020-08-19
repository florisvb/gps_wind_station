import numpy as np
import os
import matplotlib.pyplot as plt
import pandas as pd
import calendar
import scipy.interpolate
import argparse

import scipy.stats # generic linear regression
from sklearn import linear_model # some fancy stuff

import load_windgps_data_to_pandas

def fix_millis_errors(df):
    idx = np.where(  np.diff(df.millis.astype(int))<0 )
    df.loc[idx[0] + 1, 'millis'] = np.nan
    df.millis.interpolate(inplace=True)
    df.millis = df.millis.astype(int)
    return df

def fix_gps_date(df, correct_year=2020, base_year=2000):
    df = df[df.gps_date != 0]
    df = df[df.gps_time != 0]
    
    df['gps_date_str'] = df.gps_date.astype(str)
    df['gps_date_str'] = [s.zfill(6) for s in df.gps_date_str.values]

    df['year'] = np.array([ base_year + int(s[4:6]) for s in df['gps_date_str']])
    df['month'] = [ int(s[2:4]) for s in df.gps_date_str]
    df['day'] = [ int(s[0:2]) for s in df.gps_date_str]

    idx = np.where( df['year'].values != correct_year )[0]

    df.loc[ df.index[idx].values, 'year'] = np.nan
    df['year'].interpolate(inplace=True)

    df.loc[ df.index[idx].values, 'month'] = np.nan
    df['month'].interpolate(inplace=True)

    df.loc[ df.index[idx].values, 'day'] = np.nan
    df['day'].interpolate(inplace=True)

    return df

def calc_interpolated_epoch_time(df):
    df.gps_time /= 100
    df.gps_time = df.gps_time.astype(int)
    df['gps_time_str'] = df.gps_time.astype(str)
    df['gps_time_str'] = [s.zfill(6) for s in df.gps_time_str.values]
    df['hour'] =   [ int(s[0:2]) for s in df['gps_time_str']]
    df['minute'] = [ int(s[2:4]) for s in df['gps_time_str']]
    df['second'] = [ int(s[4:6]) for s in df['gps_time_str']]

    
    time_diff = np.hstack( (0, np.diff(df.gps_time)) )
    idx = np.where(time_diff==1)[0]
    
    # Note: GPS time is in GMT (greenwich time)
    millis_vals = []
    epoch_vals = []

    for i in idx:
        t = calendar.timegm((int(df.year.iloc[i]), 
                             int(df.month.iloc[i]), 
                             int(df.day.iloc[i]), 
                             int(df.hour.iloc[i]), 
                             int(df.minute.iloc[i]), 
                             int(df.second.iloc[i]), 
                             -1, -1, -1) )
        millis_vals.append(df.millis.iloc[i])
        epoch_vals.append(t)

    millis_vals = np.array(millis_vals)
    epoch_vals = np.array(epoch_vals)
    
    ransac = linear_model.RANSACRegressor(residual_threshold=0.08) # threshold determines how many inliers/outliers
    ransac.fit( millis_vals.reshape(1, -1).T, epoch_vals  )
    inlier_mask = ransac.inlier_mask_
    outlier_mask = np.logical_not(inlier_mask)
    
    result = scipy.stats.linregress(millis_vals[inlier_mask], epoch_vals[inlier_mask])
    
    interpolated_epoch_time = df.millis*result.slope + result.intercept
    
    df['time_epoch'] = interpolated_epoch_time
    
    return df

def parse_wind_string(row, key):
    s = row['wind']
    s = s.lstrip().rstrip()
    s = s.split(b'\x00')[0]
    s = s.rstrip()
    
    ls = []
    for ss in s.split(b' '):
        if len(ss) > 0:
            ls.append(ss)
            
    try:
        labels = ls[0::2]
        labels = [l.decode('utf-8') for l in labels]

        vals = ls[1::2]
        vals = np.array(vals).astype(float)
        
        d = {labels[i]: vals[i] for i in range(len(vals)) }
    
        return d[key]
    except:
        return np.nan

def parse_and_save_several_wind_strings(df, wind_strings=['S2', 'D']):
    print('Warning: This takes a while')

    for key in wind_strings:
        df[key] = df.apply(parse_wind_string, axis=1, args=(key,))

    return df


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--data', type=str, help="the directory to the data")
    parser.add_argument('--out', type=str, help="the directory in which to save the resulting hdf file")
    parser.add_argument('--year', type=int, help="correct year e.g. 2020")
    
    args = parser.parse_args()

    # load
    df = load_windgps_data_to_pandas.load_data_from_directory(args.data)

    # fix
    df = fix_gps_date(df, correct_year=args.year)
    df = calc_interpolated_epoch_time(df)
    df = parse_and_save_several_wind_strings(df, wind_strings=['S2', 'D'])

    # save
    datestr = str(int(df.year[0])) + str(int(df.month[0])) + str(int(df.day[0])) + '_' + str(int(df.hour[0])) + str(int(df.minute[0])) + str(int(df.second[0]))
    fname = datestr + '_' + 'windgps_data.hdf'
    full_fname = os.path.join(args.out, fname)
    df.to_hdf(full_fname, 'windgps')