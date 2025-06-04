import os
import sys
import numpy as np
from scipy import interpolate

try:
    import pandas as pd
    import matplotlib.pyplot as plt
    import seaborn as sns
    from scipy import stats
except ImportError as e:
    print(f"Error importing required packages: {e}")
    print(f"Python path: {sys.path}")
    sys.exit(1)

# Get the absolute path to the CSV files
current_dir = os.path.dirname(os.path.abspath(__file__))
dsp_data_path = os.path.join(current_dir, 'dspData.csv')
esp32_data_path = os.path.join(current_dir, 'ESP32Data.csv')

try:
    # Read the CSV files
    print(f"Reading DSP data from: {dsp_data_path}")
    dsp_data = pd.read_csv(dsp_data_path)
    print(f"Reading ESP32 data from: {esp32_data_path}")
    esp32_data = pd.read_csv(esp32_data_path)
except Exception as e:
    print(f"Error reading CSV files: {e}")
    sys.exit(1)

# Adjust DSP timestamps by subtracting 2
dsp_data['timestamp_ms'] = dsp_data['timestamp_ms'] - 2

def calculate_distance_weight(query_time, reference_times, max_distance=10):
    """Calculate weight based on distance to nearest actual measurement.
    Weight decreases exponentially with distance to nearest real measurement.
    max_distance: maximum time difference (in ms) after which weight becomes very close to 0"""
    distances = np.abs(reference_times - query_time)
    min_distance = np.min(distances)
    # Exponential decay instead of linear
    weight = np.exp(-2 * min_distance / max_distance)  # More aggressive decay, 0 distance = 1, 10 distance = 0.135.
    return weight

# Create interpolation functions for ESP32 data
esp32_left_interp = interpolate.interp1d(esp32_data['timestamp_ms'], 
                                        esp32_data['left_brightness'],
                                        bounds_error=False,
                                        fill_value="extrapolate")
esp32_right_interp = interpolate.interp1d(esp32_data['timestamp_ms'], 
                                         esp32_data['right_brightness'],
                                         bounds_error=False,
                                         fill_value="extrapolate")

# Get interpolated ESP32 values and calculate weights
esp32_left_interpolated = esp32_left_interp(dsp_data['timestamp_ms'])
esp32_right_interpolated = esp32_right_interp(dsp_data['timestamp_ms'])

# Calculate weights for each DSP timestamp
weights = np.array([calculate_distance_weight(t, esp32_data['timestamp_ms'].values) 
                   for t in dsp_data['timestamp_ms']])

# Calculate weighted correlations
def weighted_correlation(x, y, weights):
    """Calculate weighted Pearson correlation"""
    weighted_mean_x = np.average(x, weights=weights)
    weighted_mean_y = np.average(y, weights=weights)
    
    weighted_cov = np.average((x - weighted_mean_x) * (y - weighted_mean_y), weights=weights)
    weighted_var_x = np.average((x - weighted_mean_x) ** 2, weights=weights)
    weighted_var_y = np.average((y - weighted_mean_y) ** 2, weights=weights)
    
    return weighted_cov / np.sqrt(weighted_var_x * weighted_var_y)

# Calculate both regular and weighted correlations
left_corr = stats.pearsonr(dsp_data['left_brightness'], esp32_left_interpolated)
right_corr = stats.pearsonr(dsp_data['right_brightness'], esp32_right_interpolated)

left_weighted_corr = weighted_correlation(dsp_data['left_brightness'], esp32_left_interpolated, weights)
right_weighted_corr = weighted_correlation(dsp_data['right_brightness'], esp32_right_interpolated, weights)

# Create first figure for time series data
fig1 = plt.figure(figsize=(12, 12))

# Adjust the subplot parameters for better spacing
plt.subplots_adjust(top=0.85, hspace=0.3)

# Plot DSP Data
ax1 = plt.subplot(2, 1, 1)
ax1.plot(dsp_data['timestamp_ms'], dsp_data['left_brightness'], label='Left', alpha=0.7)
ax1.plot(dsp_data['timestamp_ms'], dsp_data['right_brightness'], label='Right', alpha=0.7)
ax1.set_title('DSP Data: Brightness over Time', pad=20, y=0.95)
ax1.set_xlabel('Time (ms)', labelpad=10)
ax1.set_ylabel('Brightness', labelpad=10)
ax1.legend(loc='upper right')
ax1.grid(True)

# Plot ESP32 Data
ax2 = plt.subplot(2, 1, 2)
ax2.plot(esp32_data['timestamp_ms'], esp32_data['left_brightness'], label='Left', alpha=0.7)
ax2.plot(esp32_data['timestamp_ms'], esp32_data['right_brightness'], label='Right', alpha=0.7)
ax2.set_title('ESP32 Data: Brightness over Time', pad=20, y=0.95)
ax2.set_xlabel('Time (ms)', labelpad=10)
ax2.set_ylabel('Brightness', labelpad=10)
ax2.legend(loc='upper right')
ax2.grid(True)

# Add a main title to the first figure
fig1.suptitle('Time Series Analysis: DSP and ESP32', fontsize=16, y=0.98)

# Create second figure for correlation analysis
fig2 = plt.figure(figsize=(10, 8))
ax3 = fig2.add_subplot(111)

# Plot correlation for both channels
scatter_left = ax3.scatter(dsp_data['left_brightness'], esp32_left_interpolated, 
                          c=weights, cmap='RdYlBu_r', alpha=0.6, 
                          label=f'Left Channel (r={left_corr[0]:.4f}, w_r={left_weighted_corr:.4f})')
scatter_right = ax3.scatter(dsp_data['right_brightness'], esp32_right_interpolated, 
                           c=weights, cmap='RdYlBu_r', alpha=0.6, marker='s',
                           label=f'Right Channel (r={right_corr[0]:.4f}, w_r={right_weighted_corr:.4f})')

# Add colorbar with adjusted label
cbar = plt.colorbar(scatter_left)
cbar.set_label('Weight (Distance to nearest ESP32 sample)', rotation=270, labelpad=15)

ax3.set_title('Correlation Analysis: DSP vs ESP32', pad=20)
ax3.set_xlabel('DSP Brightness', labelpad=10)
ax3.set_ylabel('ESP32 Brightness (Interpolated)', labelpad=10)
ax3.grid(True)
ax3.legend()

# Adjust layout
plt.tight_layout()

# Show all plots
plt.show()

# Print detailed statistics
print("\nCorrelation Analysis:")
print("\nLeft Channel:")
print(f"Regular correlation: {left_corr[0]:.6f}")
print(f"Weighted correlation: {left_weighted_corr:.6f}")
print(f"Difference: {left_weighted_corr - left_corr[0]:.6f}")

print("\nRight Channel:")
print(f"Regular correlation: {right_corr[0]:.6f}")
print(f"Weighted correlation: {right_weighted_corr:.6f}")
print(f"Difference: {right_weighted_corr - right_corr[0]:.6f}")

# Print weight statistics
print("\nWeight Statistics:")
print(f"Average weight: {weights.mean():.3f}")
print(f"Minimum weight: {weights.min():.3f}")
print(f"Maximum weight: {weights.max():.3f}")
print(f"Percentage of points with weight > 0.8: {(weights > 0.8).mean() * 100:.1f}%")
print(f"Percentage of points with weight < 0.2: {(weights < 0.2).mean() * 100:.1f}%")

# Calculate and print additional statistics
print("\nValue Ranges:")
print("\nLeft Channel:")
print(f"DSP: Min={dsp_data['left_brightness'].min():.3f}, Max={dsp_data['left_brightness'].max():.3f}")
print(f"ESP32: Min={esp32_left_interpolated.min():.3f}, Max={esp32_left_interpolated.max():.3f}")

print("\nRight Channel:")
print(f"DSP: Min={dsp_data['right_brightness'].min():.3f}, Max={dsp_data['right_brightness'].max():.3f}")
print(f"ESP32: Min={esp32_right_interpolated.min():.3f}, Max={esp32_right_interpolated.max():.3f}")

# Print some timing statistics
print("\nTimestamp Statistics:")
print("DSP timestamps range:", dsp_data['timestamp_ms'].min(), "to", dsp_data['timestamp_ms'].max())
print("ESP32 timestamps range:", esp32_data['timestamp_ms'].min(), "to", esp32_data['timestamp_ms'].max())

# Calculate and print interpolation statistics
time_diffs = np.diff(dsp_data['timestamp_ms'])
print("\nInterpolation Statistics:")
print(f"Average time between DSP samples: {time_diffs.mean():.2f} ms")
print(f"Max time between DSP samples: {time_diffs.max():.2f} ms")
print(f"Min time between DSP samples: {time_diffs.min():.2f} ms") 