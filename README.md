# 🏔️ Terraverse: Procedural Low-Poly Expedition Engine

**Terraverse** is an infinite, procedural open-world offroad exploration engine built from scratch in **Unreal Engine 5** using **C++**, **Custom HLSL Shaders**, and **Chaos Vehicle Physics**.

The project follows a strict **"0-Asset" philosophy**: every single asset—from mountain heights to low-poly pine trees, boulders, vehicles, and multi-layered ground materials—is generated purely via algorithms, mathematics, and engine primitives.

---

## 🎬 Showcase Demos

| Demonstration | Description | Video Link |
| :--- | :--- | :---: |
| **Lush Alpine Valley** | 0-asset pine tree clustering, 3-meter mosaic grass, and valley navigation | [Watch Meadow.mp4](docs/videos/Meadow.mp4) |
| **High Alpine & Snowy Peaks** | Dynamic snow line elevation, exposed cliff rocks, and high-altitude driving | [Watch HighMeadow.mp4](docs/videos/HighMeadow.mp4) |
| **Multi-Biome Transition** | Seamless macro-climate transition between Alpine Meadow and Arid Canyon | [Watch CanyonMeadow.mp4](docs/videos/CanyonMeadow.mp4) |

---

## 🏛️ Technical Architecture & Key Systems

### 1. Multi-Threaded Async Chunk Streaming (`WTChunkManager` & `WTChunkActor`)
- **Zero-Hitch Background Generation:** Vertex math, normals, finite-difference derivations, and climate sampling are offloaded to Worker Threads (`EAsyncExecution::ThreadPool`).
- **Deferred Spawning Pipeline:** Player pawn is safely held until the initial collision ring is baked—eliminating falling-through-world physics bugs.
- **In-Editor Preview (`CallInEditor`):** Level designers can generate and inspect the world directly in viewport without pressing Play.

### 2. Multi-Tier LOD & Seam-Free Skirt System
- **Distance-Based LOD:** 
  - Immediate vicinity: High detail cell size, full trimesh collision).
  - Mid-range: Medium detail cell size, collision disabled).
  - Horizon: Ultra-lightweight silhouette cell size).
- **Procedural Skirts:** Vertical geometry extruded downward along chunk boundaries to completely eliminate LOD T-junction seams/cracks.
- **Horizon Atmospheric Blending:** Exponential Height Fog calibrated to sky inscattering, blending distant low-poly mountains seamlessly into the skybox.

### 3. Real-World Elevation Data Integration (`WTOnlineHeightSource`)
- Streams real-world elevation karos on the fly from **AWS Open Data (Terrarium DEM format)**.
- Raw RGB png tiles decoded into centimeter-accurate elevations.
- Thread-safe local disk caching (`Saved/TerrainCache/`) preventing redundant network calls.

### 4. Custom HLSL Shading & 16-Bit Float Climate LUT
- **16-bit Float Climate Texture:** A transient texture generated in C++ dynamically encoding grass, dirt, and rock colors alongside snow elevation thresholds.
- **HLSL Custom Node Pipeline:**
  - 3-tier slope blending: Steep Cliffs (Rock) -> Slopes (Dirt) -> Flat Valleys (Grass).
  - **Procedural 3m Mosaic:** Deterministic 2D grid hashing breaking flat surfaces into stylized low-poly faceted blocks without textures.
  - **Physical Lapse Rate Snow:** High-altitude freezing calculation with slope-aware snow clinging and Fresnel ice glint.

### 5. Deterministic HISM Prop Ecosystem (Phase 4)
- **Hierarchical Instanced Static Mesh (HISM):** Thousands of props drawn in a single draw call.
- **Ecological Rules (`FWTPropRule`):** Biome-aware constraints governing slope limits, tree-lines (snow cutoff), and multi-mesh variations.
- **Organic Clustering:** Low-frequency 2D Perlin noise masks creating dense forest groves separated by natural driving glades.
- **Procedural Variation:** Random yaw + dynamic tilt/lean angles simulating natural wind and ground settlement.

### 6. Tuned 4WD Expedition Vehicle (Phase 5)
- Custom-built minimalist truck chassis and wheels crafted using basic primitives.
- Balanced 4WD limited-slip drivetrain, lowered center of mass, and anti-slip idle braking for steep rock-crawling.
- Dual camera rig: Seamless toggle between 3rd-person chase camera and 1st-person cabin cockpit view (`C` key).

---

## 🗺️ Project Roadmap

- [x] **Phase 1:** Vertical Slice (Synchronous chunk, basic Chaos vehicle).
- [x] **Phase 2:** Infinite Async Streaming, Skirts, CallInEditor & LOD system.
- [x] **Phase 3:** Biome engine, AWS Terrarium DEM stream, custom HLSL shader.
