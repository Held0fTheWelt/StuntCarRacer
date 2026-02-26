Async Spline Track Builder – Documentation
=========================================

1. Overview
-----------

The **Async Spline Track Builder** lets you procedurally generate road/track meshes along a spline in the Unreal Editor.

It is designed for:

- Race tracks and roads that follow a spline
- Non-blocking, **async** mesh generation in the editor (no long freezes on large tracks)
- Optional **extra meshes** such as guard rails, barriers, or side objects
- Optional **landscape snapping & deformation** to blend the terrain to your track
- Optional **data asset I/O** for saving and loading spline point lists

The core class is:

- `ASplineGeneratingActor` (C++ actor, exposed to editor)
- `FTrackSplineData` / `FSplineMeshComponentData` (data structs used to configure each spline segment)


2. Requirements & Setup
-----------------------

2.1. Enabling the Plugin
~~~~~~~~~~~~~~~~~~~~~~~~

1. Open **Edit → Plugins**.
2. Enable the plugin that contains `ASplineGeneratingActor` (e.g. *AsyncSplineBuilder* or *Collections* depending on your setup).
3. Restart the editor if prompted.


2.2. Adding the Actor to a Level
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. In the **Content Browser**, locate the `ASplineGeneratingActor` class (via *Place Actors* panel or your plugin’s folder).
2. Drag **Spline Generating Actor** into the level.
3. Select the actor in the level to see all options in the **Details** panel.


3. Basic Workflow
-----------------

3.1. Assign Road Meshes
~~~~~~~~~~~~~~~~~~~~~~~

Under **Mesh Setup** on the actor:

- `StartMesh` – Mesh used at the **very beginning** of the track.
- `MainMesh` – Mesh used for **repeating segments** in the middle.
- `EndMesh` – Mesh used at the **end** of the track.

All three meshes **must** be assigned, otherwise the build is aborted and an error is logged.

Ideally:

- All three meshes have consistent dimensions along **X** (length).
- The meshes are modeled so they tile cleanly when placed back-to-back.


3.2. Editing the Spline
~~~~~~~~~~~~~~~~~~~~~~~

In the actor’s **Default** / **TrackTools** categories:

- `TrackSpline` is the built-in `USplineComponent` that defines the path.
- Use normal spline editing tools in the viewport:
  - Move, add, or delete spline points.
  - Rotate/twist points to shape banking or roll.
- `bClosedLoop` – If enabled, the last point connects back to the first, creating a loop track.

You can also enable:

- `bShowPointNumbers` – Shows the spline point indices as floating text.
- `bShowSegmentNumbers` – Shows segment indices between points.
- `SplineVisualizationWidth` – Controls width of the editor spline visualization if `bShowSplineVisualizationWidth` is true.


3.3. Building the Track
~~~~~~~~~~~~~~~~~~~~~~~

There are two main ways to build the meshes:

Auto-Rebuild (Recommended for iteration)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- `bAutoRebuildOnConstruction` (TrackTools|Async)
  - When true, changing spline points or settings will schedule an automatic rebuild.
  - Rebuild is handled asynchronously to avoid long blocking operations.

Manual Rebuild
^^^^^^^^^^^^^^

Under **TrackTools**:

- `RebuildTrack` (Call in Editor button)
  - Manually triggers a rebuild.
- `CancelAsyncBuild`
  - Cancels any ongoing or pending async build.


4. Async vs. Sync Build
-----------------------

4.1. Async Build
~~~~~~~~~~~~~~~~

- `bUseAsyncBuild` (TrackTools|Async)
  - When true, the track is built **over multiple ticks** instead of in a single blocking call.
- `SegmentsPerTick`
  - Number of spline segments processed per editor tick.
  - Lower values → smoother editor experience, longer build time.
  - Higher values → faster build, but more impact per frame.

Async flow:

1. `RequestBuild()` is called (via auto rebuild or `RebuildTrack`).
2. Actor clears old components and recalculates `TrackSplineData`.
3. Build is scheduled (debounced) and started in `Tick`.
4. `BuildNextSegments()` processes a batch of segments each tick.
5. `FinishBuild_Internal()` finalizes the build, updates the spline, and draws debug labels.


4.2. Sync Build
~~~~~~~~~~~~~~~

If `bUseAsyncBuild` is false:

- The actor builds the entire track immediately in `RequestBuild()` via:
  - `AddRoadAndExtraMeshesToSpline()`
- This may block the editor for large tracks but is simpler and deterministic.


5. Landscape Integration
------------------------

The actor can **interact with landscapes** to align and/or deform the terrain.


5.1. Snapping Meshes to Landscape
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Under **Options**:

- `bSnapMeshesToLandscape`
  - When true, each generated spline mesh searches for a landscape beneath its start/end.
  - Uses `LineTraceHitLandscape()` with:
    - `ObjectsToHitForLandscapeLineTrace` (object types considered as landscape)
    - `LineTraceLength` (vertical trace reach)

This is useful to keep the road visually glued to terrain undulations.


5.2. Snapping Spline Points to Landscape
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Under **TrackTools**:

- `bSnapPointsToLandscape`
  - When true, spline control points are moved onto the landscape surface.
- `bTangentPointsUpdate`
  - If enabled, tangents are recalculated using the impact normal and right vector.

The operation is handled in:

- `SnapToLandscape()`
- Uses `LineTraceHitLandscape()` for each spline point.


5.3. Landscape Deformation (Road Cutting/Stamping)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Under **Landscape**:

- `Landscape` – The `ALandscapeProxy` to deform.
- `PaintLayer` – Optional `ULandscapeLayerInfoObject` to paint along the track.
- `LandscapeEditLayerName` – Name of the edit layer.
- `FallOff` – Outer falloff radius for deformation.
- `NumberOfSubdivisionsForDeform` – Resolution of landscape spline sampling.
- `bRaiseHeights` / `bLowerHeights` – Whether to allow raising and/or lowering terrain.

When `bDeformLandscape` is true and a valid Landscape is set:

- For each main road segment, a temporary spline is created and passed to:
  - `Landscape->EditorApplySpline(...)`
- This cuts/stamps the landscape to follow the road width and falloff.

Use this to carve the road into the terrain and avoid floating/embedded sections.


6. Extra Meshes (Guard Rails, Side Objects, etc.)
-------------------------------------------------

Each segment of the track uses a `FTrackSplineData` entry:

- `MeshInstances` – Number of main road meshes in that segment.
- `SegmentLength` – Length of the segment.
- `RoadMeshLength` – Final mesh coverage length.
- `ExtraMeshStart` – Extra meshes for the **first instance** in the segment.
- `ExtraMesh` – Extra meshes for **middle instances**.
- `ExtraMeshEnd` – Extra meshes for the **last instance**.
- `ExtraMeshOffset` – Per extra mesh lateral offset (in local space).

The actor uses:

- `BuildExtraSplineMeshComponent()` to create these side meshes.

Under **TrackTools**:

- `bMirrorExtraMesh` – Flips X scale of extra meshes, useful to mirror rails or side objects.
- `ExtraMeshOffset` (per segment) can be used to offset objects outward from the road edge.

Typical use cases:

- Guard rails left/right
- Fences, posts, lamp poles
- Side walls inside tunnels


7. Data Asset Integration (SplinePointList)
-------------------------------------------

The actor can **read and write** spline point data to a data asset of type `USplinePointListAsset`.


7.1. Creating a Spline Point List Asset
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. In **Content Browser**, create a new `SplinePointListAsset` (from your plugin’s asset types).
2. Assign it to `SplinePointList` property in the actor under **Data**.


7.2. Reading From Asset
~~~~~~~~~~~~~~~~~~~~~~~

- `bReadFromDataAsset`:
  - When enabled, the actor will:
    - Clear current spline points.
    - Rebuild them from `SplinePointList->PointList`.
  - After loading, `bReadFromDataAsset` resets to false.

Useful for:

- Keeping track shapes as assets independent from level actors.
- Reusing the same track profile in multiple levels.


7.3. Writing To Asset
~~~~~~~~~~~~~~~~~~~~~

- `bWriteToDataAsset`:
  - When enabled, the actor will:
    - Serialize current spline point transforms and tangents into `SplinePointList->PointList`.
  - After writing, `bWriteToDataAsset` resets to false.

Useful for:

- Authoring a track in the level, then saving it as reusable data.
- Versioning multiple track variations.


8. Debugging & Understanding What Happens
-----------------------------------------

8.1. Logs
~~~~~~~~~

All important events and errors are logged via the `LogAsyncSplineBuilder` category:

- Missing meshes (Start/Main/End)
- Null spline references
- Landscape trace failures
- Async start/finish and cancellation

Use **Output Log** and filter by:

    LogAsyncSplineBuilder


8.2. Visual Debugging
~~~~~~~~~~~~~~~~~~~~~

- Enable `bShowPointNumbers` and/or `bShowSegmentNumbers` to see indices.
- `DebugTextWorldSize` controls the text size.
- `bShowSplineVisualizationWidth` and `SplineVisualizationWidth` show road width.

These visuals are created via:

- `DebugTrackSpline()`
- `ShowSegmentNumbers()`


9. Example Usage in a Playable Level
------------------------------------

A simple example setup in a demo level:

1. Place `ASplineGeneratingActor` in the level.
2. Assign basic road meshes (Start/Main/End).
3. Shape the spline around your level layout.
4. Enable:
   - `bUseAsyncBuild = true`
   - `bAutoRebuildOnConstruction = true`
   - `bShowSegmentNumbers = true` (for debugging)
5. Optionally:
   - Assign `Landscape`, `PaintLayer`, and enable `bDeformLandscape` for terrain integration.
6. Hit **Play**:
   - The generated track meshes are static scene geometry and can be driven on / walked over.

You can also include:

- An in-level info board (using `UTextRenderComponent`) explaining how to:
  - Select the actor.
  - Edit the spline.
  - Press `RebuildTrack`.
  - Adjust async settings.


10. Troubleshooting
-------------------

Track doesn’t appear / nothing is generated
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Check:

- `StartMesh`, `MainMesh`, `EndMesh` are not `None`.
- The spline has at least 2 points.
- `SplineSegments` > 0 (see logs).


Editor freezes when building track
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Enable `bUseAsyncBuild`.
- Reduce `SegmentsPerTick` to 1 or 2.


Road floats above or sinks into terrain
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Adjust:

- `SplineZOffset`
- `SplineZOffsetLandscapeSnapCorrection`

Use `bSnapMeshesToLandscape` and ensure:

- `ObjectsToHitForLandscapeLineTrace` includes your landscape object type.
- `LineTraceLength` is long enough to reach the landscape.


Landscape is not deforming
~~~~~~~~~~~~~~~~~~~~~~~~~~

Ensure:

- `Landscape` is set to a valid `ALandscapeProxy`.
- `bDeformLandscape` is true **before** building.
- `NumberOfSubdivisionsForDeform` > 0.
- `FallOff` is appropriate for your scale.
