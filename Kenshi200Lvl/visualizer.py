import numpy as np
import math
import matplotlib.pyplot as plt

FadeLevel = 50.0
MaxLevel = 201.0

# const
factor1 = 1.0
factor2 = 101.0

def get_adjustment(value, factor1, factor2):
    invFactor2 = 1.0 / factor2 if factor2 != 0 else np.inf

    if value < FadeLevel:
        normalizedDifference = (factor2 - value) * invFactor2
        val = normalizedDifference * normalizedDifference
    else:
        normalizedProgress = (value - FadeLevel) / (MaxLevel - FadeLevel)
        baseDifficulty = (factor2 - FadeLevel) * invFactor2
        baseDifficulty *= baseDifficulty
        val = baseDifficulty * (1.0 - normalizedProgress) * (1.0 - normalizedProgress)

    if (value > MaxLevel):
        return 0.0
    if not np.isnan(val) and val > 0.0 and factor1 > 0.0 and factor1 <= 20.0 and val <= 20.0:
        return val * factor1
    else:
        return 0.0

values = np.linspace(0, MaxLevel + math.floor(MaxLevel / 5), 400)
adjustments = [get_adjustment(v, factor1, factor2) for v in values]

plt.figure(figsize=(9, 6))
plt.plot(values, adjustments, color='royalblue', linewidth=2)
plt.axvline(FadeLevel, color='red', linestyle='--', linewidth=1.2, alpha=0.2)
plt.axvline(MaxLevel, color='gray', linestyle=':', linewidth=1.2)
plt.title("Leveling in Kenshi")
plt.xlabel("Current Level")
plt.ylabel("XP Gain")
plt.legend()
plt.grid(True, linestyle='--', alpha=0.5)
plt.tight_layout()
plt.ylim(bottom=0)
plt.show()