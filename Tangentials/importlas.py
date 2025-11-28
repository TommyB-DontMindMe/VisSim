import numpy
import laspy

# Angi filnavn
input_file = '32-1-517-155-31.laz'           # Din LAZ-fil
output_file = 'lasdata.txt'   # Utdatafil i ASCII-format

# Les LAZ-filen
las = laspy.read(input_file, laz_backend=laspy.LazBackend.Laszip)

# Decimering implementert i Python ved hjelp av Anthropic Claude
# Kommentarer er mine egne
# Split i x,y,z komponenter
las_x = numpy.array(las.x)
las_y = numpy.array(las.y)
las_z = numpy.array(las.z)

# Finn de lavest og høyeste X og Z koordinatene
x_min, x_max = las_x.min(), las_x.max()
z_min, z_max = las_z.min(), las_z.max()

# Finn koordinater relative til "minste" punkt og rundt ned til helt tall
x_idx = numpy.floor((las_x - x_min) / 5).astype(int)
z_idx = numpy.floor((las_z - z_min) / 5).astype(int)

# Konverter 2d til 1d koordinater 
n_cols = x_idx.max() + 1
grid_ids = x_idx + z_idx * n_cols

# Lag en liste av alle unike koordinater, dvs. Alle XY koordinater for celler med minst et punkt
unique_ids = numpy.unique(grid_ids)

# Leg et array med en rekke for hver celle og 3 kolonner i hver rekke
decimated_points = numpy.zeros((len(unique_ids), 3))

# Fyll in XYZ verdier til alle punkter som passer I hver celle
for i, grid_id in enumerate(unique_ids):
    mask = grid_ids == grid_id    # Bruk en boolean mask for a filtrere etter 1d koordinater fra tidligere
    decimated_points[i, 0] = las_x[mask].mean()
    decimated_points[i, 1] = las_y[mask].mean()
    decimated_points[i, 2] = las_z[mask].mean()

# Skriv til ASCII-fil

with open(output_file, 'w') as f:
    las_num = len(decimated_points)
    f.write(f"{las_num} \n")
    for x, y, z in decimated_points:
        f.write(f"{x} {y} {z} \n")

print(f"LAZ-filen '{input_file}' er eksportert til ASCII-format i filen '{output_file}'.")
