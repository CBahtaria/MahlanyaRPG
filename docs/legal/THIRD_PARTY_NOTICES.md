# Third-Party Notices — Mahlanya

This file lists third-party software, data, and toolchains used in the development and distribution of Mahlanya. Each entry identifies the component, its copyright holder, and the license under which it is used. Where the full license text is required by the license terms, it is reproduced below or incorporated by reference.

---

## 1. Unreal Engine 5

**Copyright:** Copyright © 1998–2026 Epic Games, Inc. All rights reserved.

**License:** Unreal Engine End User License Agreement (EULA)

Mahlanya is built using Unreal Engine 5, which is licensed under the Unreal Engine EULA. Unreal Engine is free to use for development and for products that gross less than USD 1,000,000 in lifetime revenue. Royalties apply above that threshold in accordance with the EULA terms. The full text of the Unreal Engine EULA is available at:

https://www.unrealengine.com/en-US/eula/unreal

**Notable components covered under this license:**
- Core engine runtime (UE5 Editor and runtime modules)
- Nanite virtualized geometry system
- Lumen global illumination system
- Chaos physics solver
- MetaSounds audio framework
- Procedural Content Generation (PCG) Framework
- Gameplay Ability System (GAS)
- Online Subsystem (base and Utils)
- World Partition streaming system
- Control Rig and Motion Warping

---

## 2. Steamworks SDK

**Copyright:** Copyright © Valve Corporation. All rights reserved.

**License:** Steamworks SDK License (Valve Corporation)

The Steamworks SDK (version 1.58) is used for Steam platform integration including authentication, achievements, Steam Cloud save synchronization, and Steam Networking. Use of the Steamworks SDK is subject to the Steamworks SDK License Agreement, which is accepted via the Valve partner portal. The Steamworks SDK may not be redistributed or used outside the context of Steam-distributed products without Valve's permission.

SDK License terms: https://partner.steamgames.com/documentation/sdk_access_rules

---

## 3. Copernicus DEM (Digital Elevation Model)

**Copyright:** Contains modified Copernicus DEM data © DLR e.V. (2021–2023) and © Airbus Defence and Space GmbH (2021–2023) provided under COPERNICUS by the European Union and ESA. All rights reserved.

**License:** Copernicus DEM — Global and European Digital Elevation Model (ESA Open License)

The terrain data underlying the Eswatini world map is derived from the Copernicus Digital Elevation Model (COP-DEM), a product of the European Space Agency and processed by Airbus Defence and Space. It is used under the ESA Open License for Copernicus Data, which permits free use, reproduction, and distribution with attribution.

Full license text: https://spacedata.copernicus.eu/en/web/guest/collections/copernicus-digital-elevation-model

**Attribution required by license:**
"Contains modified Copernicus DEM data © DLR e.V. (2021-2023) and © Airbus Defence and Space GmbH (2021-2023) provided under COPERNICUS by the European Union and ESA; all rights reserved."

---

## 4. Zig Compiler Toolchain

**Copyright:** Copyright © 2015–2026 Andrew Kelley and Zig Software Foundation contributors.

**License:** MIT License

The Zig compiler toolchain (https://ziglang.org/) is used in the MahlanyaRPG data pipeline for performance-critical processing tasks. Zig is distributed under the MIT License:

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 5. Python 3.11+

**Copyright:** Copyright © 2001–2026 Python Software Foundation. All rights reserved.

**License:** Python Software Foundation License Version 2 (PSF License)

Python (https://www.python.org/) version 3.11 or later is used for pipeline scripting, configuration validation, and data processing tooling. Python is distributed under the Python Software Foundation License Version 2.

Full license text: https://docs.python.org/3/license.html

The PSF License permits use, reproduction, modification, and distribution in source and binary forms, with or without modification, provided that the PSF copyright notice and this permission notice are included in all copies or substantial portions. This product includes software developed by the Python Software Foundation (https://www.python.org/).

---

## 6. GDAL — Geospatial Data Abstraction Library

**Copyright:** Copyright © 1998–2026 Frank Warmerdam, Even Rouault, and contributors.

**License:** MIT/X License

GDAL (https://gdal.org/) is used in the terrain data pipeline for processing geospatial raster and vector data (DEM reprojection, coordinate transformation, terrain tile generation). GDAL is distributed under the MIT/X License:

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

GDAL incorporates a number of optional libraries which may be subject to their own licenses. Refer to https://gdal.org/en/stable/development/rfc/rfc66_randomlayer.html and https://trac.osgeo.org/gdal/wiki/LicenseLists for a complete list of GDAL's optional dependency licenses.

---

*This file was last updated on 2026-07-01. If you believe a third-party component has been omitted or incorrectly attributed, please contact charleskris9@gmail.com.*
