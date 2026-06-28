#include "ArchaeologyOverlayActor.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AArchaeologyOverlayActor::AArchaeologyOverlayActor()
    : AshPitMaterial(nullptr)
    , PostHoleMesh(nullptr)
{
    PrimaryActorTick.bCanEverTick = false;

    // Root scene component
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(RootSceneComponent);

    // Ash pit decal — 2m diameter ground projection
    AshPitDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("AshPitDecal"));
    AshPitDecal->SetupAttachment(RootSceneComponent);

    // Post-hole instanced static mesh collection
    PostHoleInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PostHoles"));
    PostHoleInstances->SetupAttachment(RootSceneComponent);

    // Ghost wireframe spline — hidden by default
    GhostWireframe = CreateDefaultSubobject<USplineComponent>(TEXT("GhostWireframe"));
    GhostWireframe->SetupAttachment(RootSceneComponent);
    GhostWireframe->SetVisibility(false);
}

void AArchaeologyOverlayActor::InitialiseOverlay(FHutID InHutID, const TArray<FVector>& CellBoundary)
{
    // 1. Store the hut identifier
    HutID = InHutID;

    // 2. Set decal material if assigned
    if (AshPitMaterial)
    {
        AshPitDecal->SetDecalMaterial(AshPitMaterial);
    }

    // 3. Ash pit ring: 2m diameter = 200cm × 200cm, shallow depth 10cm
    AshPitDecal->DecalSize = FVector(10.f, 100.f, 100.f);  // Depth, Width, Height (UE5 decal convention)

    // 4. Place post-hole mesh instances at each Voronoi cell boundary vertex
    if (PostHoleMesh)
    {
        PostHoleInstances->SetStaticMesh(PostHoleMesh);
    }

    for (const FVector& Vertex : CellBoundary)
    {
        // Slight vertical offset (+2cm) so the mesh sits just above terrain
        const FVector InstanceLocation(Vertex.X, Vertex.Y, Vertex.Z + 2.f);
        FTransform InstanceTransform;
        InstanceTransform.SetLocation(InstanceLocation);
        InstanceTransform.SetRotation(FQuat::Identity);
        InstanceTransform.SetScale3D(FVector::OneVector);

        PostHoleInstances->AddInstance(InstanceTransform);
    }

    // 5. Build ghost wireframe spline from cell boundary vertices
    GhostWireframe->ClearSplinePoints(false);
    for (int32 i = 0; i < CellBoundary.Num(); ++i)
    {
        GhostWireframe->AddSplinePoint(CellBoundary[i], ESplineCoordinateSpace::World, false);
    }

    // 6. Close the spline to complete the cell polygon
    GhostWireframe->SetClosedLoop(true);

    // Update spline after all points added
    GhostWireframe->UpdateSpline();
}

void AArchaeologyOverlayActor::SetGhostWireframeVisible(bool bVisible)
{
    GhostWireframe->SetVisibility(bVisible);
}
