// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTChunkActor.generated.h"

class UProceduralMeshComponent;
class UWTHeightSource;
class UMaterialInterface;

/** Struct for holding raw mesh data to be transferred from the Worker Thread to the Game Thread */
struct FChunkMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
};

/** One square terrain tile with render mesh + collision. Phase 1: built synchronously. */
UCLASS()
class WILDTERRAIN_API AWTChunkActor : public AActor
{
	GENERATED_BODY()
	
public:
	AWTChunkActor();

	// Builds the chunk mesh asynchronously on a worker thread, then applies it on the game thread.
	void BuildChunkAsync(const UWTHeightSource* Source, FIntPoint ChunkCoord, float ChunkSizeMeters, float CellSizeMeters, UMaterialInterface* Material);

	FIntPoint GetChunkCoord() const { return Coord; }
	bool IsBuilding() const { return bIsBuilding; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	FIntPoint Coord = FIntPoint::ZeroValue;
	bool bIsBuilding = false;

	/** Worker Thread tarafýndan iþletilen saf hesaplama fonksiyonu */
	static FChunkMeshData GenerateRawMeshData(const UWTHeightSource* Source, FIntPoint ChunkCoord,
		float ChunkSizeMeters, float CellSizeMeters);

	/** Game Thread üzerinde mesh ve collision'ý devreye alan fonksiyon */
	void ApplyMeshData_GameThread(FChunkMeshData MeshData, UMaterialInterface* Material);

};
