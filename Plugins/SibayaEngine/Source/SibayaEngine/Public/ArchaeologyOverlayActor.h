#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SibayaEngineTypes.h"
#include "ArchaeologyOverlayActor.generated.h"

// Forward declarations — avoid including heavy headers in public header
class UDecalComponent;
class UInstancedStaticMeshComponent;
class USplineComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * AArchaeologyOverlayActor
 *
 * Spawned at a destroyed hut's world location to create the archaeology layer.
 * Places three elements:
 *   1. Ground decal: ash pit ring (2m diameter, dark material)
 *   2. Post-hole static mesh instances at former Voronoi cell boundary intersections
 *   3. Ghost wireframe spline of the former cell boundary (only visible in
 *      tracking/perception mode — hidden by default)
 *
 * Designed to be Blueprint-subclassable so artists can override the mesh/material
 * references without touching C++.
 */
UCLASS(Blueprintable)
class SIBAYAENGINE_API AArchaeologyOverlayActor : public AActor
{
    GENERATED_BODY()

public:
    AArchaeologyOverlayActor();

    /**
     * Initialise the archaeology overlay for a destroyed hut.
     *
     * @param InHutID         Identifier of the destroyed hut.
     * @param CellBoundary    Polygon vertices of the former Voronoi cell in world space.
     */
    UFUNCTION(BlueprintCallable, Category = "Archaeology")
    void InitialiseOverlay(FHutID InHutID, const TArray<FVector>& CellBoundary);

    /** Show or hide the ghost wireframe (call from perception/tracking system). */
    UFUNCTION(BlueprintCallable, Category = "Archaeology")
    void SetGhostWireframeVisible(bool bVisible);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootSceneComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UDecalComponent* AshPitDecal;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UInstancedStaticMeshComponent* PostHoleInstances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USplineComponent* GhostWireframe;

    /** Ash pit decal material (assigned in Blueprint subclass or editor). */
    UPROPERTY(EditDefaultsOnly, Category = "Archaeology|Assets")
    UMaterialInterface* AshPitMaterial;

    /** Post hole static mesh (assigned in Blueprint subclass or editor). */
    UPROPERTY(EditDefaultsOnly, Category = "Archaeology|Assets")
    UStaticMesh* PostHoleMesh;

private:
    FHutID HutID;
};
