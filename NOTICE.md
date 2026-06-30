# Third-Party Notices and Attributions

**Project:** Mahlanya RPG 
**Copyright:** © 2025–2026 Charles Bartaria / BRT Inc. All Rights Reserved.

This file lists all third-party components incorporated into Mahlanya RPG,
together with their licences and required attributions. Where a licence
requires reproduction of its text, the full text is included below.

---

## 1. Terrain Data — Copernicus DEM GLO-10

**Used in:** `pipeline/terrain/acquire_dem.py`, offline pipeline 
**Source:** European Space Agency / Airbus Defence and Space 
**Access:** OpenTopography API; AWS Open Data (s3://copernicus-dem-30m) 
**Licence:** Copernicus DEM — Non-Exclusive, Royalty Free Licence 

> © DLR e.V. 2010–2014 and © Airbus Defence and Space GmbH 2014–2018
> provided under COPERNICUS by the European Union and ESA.
> All rights reserved.

The Copernicus DEM GLO-10 data is produced from the WorldDEM™ Core
elevation model and is made available under the Copernicus Open Access
Hub conditions. Attribution must appear in any publication, product, or
derivative work produced using this data.

Reference: 
*Copernicus DEM — Global and European Digital Elevation Model (COP-DEM).* 
Document DS-MKS-0112, European Space Agency, 2021.

---

## 2. Geological Data — Council for Geoscience South Africa

**Used in:** `pipeline/terrain/build_hardness_map.py` 
**Source:** Council for Geoscience, Republic of South Africa 
**Website:** https://www.geoscience.org.za 

Rock hardness coefficients are derived from published geotechnical
literature and from public geological survey vector datasets produced by
the Council for Geoscience, South Africa. These datasets are made available
under the terms of the Council's open-data policy, subject to attribution.

Attribution: *Contains geological survey data © Council for Geoscience
South Africa. Used under open-data terms for non-commercial and educational
research purposes.*

---

## 3. Climatological Data — South African Weather Service (SAWS)

**Used in:** `pipeline/atmosphere/build_weather_tables.py` 
**Source:** South African Weather Service 
**Website:** https://www.weathersa.co.za 

Historical weather summary data for Eswatini stations (Manzini, Big Bend,
Pigg's Peak, Mbabane) is derived from publicly available SAWS climatological
records. Attribution is required for any derived product.

Attribution: *Contains climatological data provided by the South African
Weather Service (SAWS). © SAWS. Used under the SAWS open-data terms.*

---

## 4. Sky Radiance Model — Hosek-Wilkie Spectral Sky Model

**Used in:** `pipeline/atmosphere/compute_sky_luts.py`, `Shaders/` 
**Authors:** Lukas Hosek, Alexander Wilkie 
**Licence:** The mathematical model and its parameterisation are described
in a peer-reviewed publication. The original C reference implementation was
released by the authors as free software (BSD-style, no-attribution-required
for the algorithm itself). Our Python and HLSL implementations are original
re-implementations.

Required citation:

> Hosek, L., & Wilkie, A. (2012). An analytic model for full spectral
> sky-dome radiance. *ACM Transactions on Graphics (TOG)*, 31(4), 1–9.
> https://doi.org/10.1145/2185520.2185591

---

## 5. D-infinity Flow Algorithm

**Used in:** `pipeline/compute/src/dinf.zig`, `pipeline/terrain/extract_rivers.py` 
**Author:** David G. Tarboton, Utah Water Research Laboratory 
**Licence:** Algorithm in the public domain; citation required.

Required citation:

> Tarboton, D. G. (1997). A new method for the determination of flow
> directions and upslope areas in grid digital elevation models.
> *Water Resources Research*, 33(2), 309–319.
> https://doi.org/10.1029/96WR03137

---

## 6. Python Dependencies

The following Python packages are used in the offline science pipeline.
All are permissively licensed and compatible with proprietary use.

| Package | Licence | URL |
|---|---|---|
| GDAL ≥ 3.8 | MIT/X11 | https://gdal.org |
| rasterio ≥ 1.3 | BSD-3-Clause | https://github.com/rasterio/rasterio |
| NumPy ≥ 1.26 | BSD-3-Clause | https://numpy.org |
| SciPy ≥ 1.12 | BSD-3-Clause | https://scipy.org |
| Numba ≥ 0.59 | BSD-2-Clause | https://numba.pydata.org |
| Shapely ≥ 2.0 | BSD-3-Clause | https://shapely.readthedocs.io |
| GeoPandas ≥ 0.14 | BSD-3-Clause | https://geopandas.org |
| pyproj ≥ 3.6 | MIT | https://pyproj4.github.io/pyproj |
| trimesh ≥ 4.0 | MIT | https://trimsh.org |
| neo4j ≥ 5.0 | Apache-2.0 | https://neo4j.com/docs/api/python-driver |
| click ≥ 8.1 | BSD-3-Clause | https://click.palletsprojects.com |
| PyYAML ≥ 6.0 | MIT | https://pyyaml.org |
| pytest ≥ 8.0 | MIT | https://pytest.org |
| pytest-cov ≥ 5.0 | MIT | https://pytest-cov.readthedocs.io |
| CuPy ≥ 13.0 (optional) | MIT | https://cupy.dev |
| aws-cdk-lib (optional) | Apache-2.0 | https://docs.aws.amazon.com/cdk |

Full licence texts for BSD-3-Clause, MIT, and Apache-2.0 are available at: 
https://opensource.org/licenses

---

## 7. Zig Programming Language

**Used in:** `pipeline/compute/` 
**Licence:** MIT 
**Copyright:** © Zig Software Foundation and contributors 
**Website:** https://ziglang.org

```
MIT License

Copyright (c) Zig contributors

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

---

## 8. Unreal Engine 5

**Used in:** `Source/`, `Plugins/`, `Config/`, `Shaders/` 
**Author:** Epic Games, Inc. 
**Licence:** Unreal Engine End User License Agreement 
**URL:** https://www.unrealengine.com/eula/unrealengine

This project is built on Unreal Engine 5 (UE5). The UE5 source code is not
distributed within this repository. Source files under `Source/` and
`Plugins/` include UE5 headers and link against UE5 libraries at build time.
Use of Unreal Engine is subject to the Unreal Engine EULA. Key terms:

- Royalty: 5% of gross revenue exceeding USD $1,000,000 per product per
  calendar year after January 1, 2024.
- Distribution of Engine Code in source form is prohibited; only
  Integrated Products (compiled games) may be distributed.

---

## 9. Historical Sources

Historical content in `pipeline/history/data/` is based on the following
scholarly and archival sources. Historical facts are not copyrightable;
the compilation and data structure are © Charles Bartaria / BRT Inc.

- **Matsapha Academic Sources** — Eswatini National Archives, Mbabane
- **Kuper, H.** (1947). *An African Aristocracy: Rank Among the Swazi.*
  Oxford University Press.
- **Booth, A.R.** (1983). *Swaziland: Tradition and Change in a Southern
  African Kingdom.* Westview Press.
- **Matsebula, J.S.M.** (1988). *A History of Swaziland.* Longman.
- **Bonner, P.** (1983). *Kings, Commoners and Concessionaires: The
  Evolution and Dissolution of the Nineteenth-Century Swazi State.*
  Cambridge University Press.

---

## 10. siSwati Language Content

siSwati is a Bantu language of the Kingdom of Eswatini. Morphological
rules, vocabulary, and cultural linguistic content used in
`pipeline/history/` and game dialogue are drawn from:

- **Rycroft, D.K.** (1981). *Concise SiSwati Dictionary.* Van Schaik.
- Consultations with native siSwati speakers.

No proprietary linguistic database is incorporated. The siSwati language
itself is not owned by any party.

---

## 11. Lotka-Volterra Equations

**Used in:** `Plugins/EcologySimulatorPlugin/` 
The Lotka-Volterra predator-prey equations are mathematical formulas
published independently by Alfred J. Lotka (1925) and Vito Volterra
(1926) and are in the public domain. No attribution is legally required;
it is included here for scientific integrity.

---

*This NOTICE.md was last updated: 2026-06-30* 
*For corrections or licensing queries: charleskris9@gmail.com*
