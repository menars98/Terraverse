// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTChunkManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTerrainReadySignature);

class AWTChunkActor;
class UWTHeightSource;
class UMaterialInterface;

/** Phase 1 test actor: drop in a level, assign a HeightSource, press Play. Replaced by the chunk manager in Phase 2. */
UCLASS()
class WILDTERRAIN_API AWTChunkManager : public AActor
{
	GENERATED_BODY()
	
public:
	AWTChunkManager();


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UWTHeightSource> HeightSource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** Zemin tamamen hazýr olduðunda GameMode'u tetikleyecek Event Dispatcher */
	UPROPERTY(BlueprintAssignable, Category = "Terrain|Events")
	FOnTerrainReadySignature OnTerrainReady;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float ChunkSizeMeters = 256.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float CellSizeMeters = 2.f;

	/** Oyuncunun etrafýnda kaç chunk yarýçapýnda dünya oluþturulacak */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "1", ClampMax = "8"))
	int32 ViewDistanceChunks = 3;

	/** Oyuncu konumunun ne sýklýkla kontrol edileceði (Saniye) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float UpdateInterval = 0.3f;

	/** Oyuncunun zemin olustugunda otomatik olarak yere oturtulmasi */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Spawning")
	bool bAutoSnapPlayerToGround = true;

	/** Yerin kac cm uzerine birakilacagi (Tekerleklerin yere gommeyi onler) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Spawning")
	float GroundSpawnOffsetCm = 150.f;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terrain")
	bool IsTerrainReady() const { return bIsTerrainReady; }

	/** Verilen X, Y koordinatýnýn zemin yüksekliðini güvenle cm cinsinden döner */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terrain")
	FVector GetGroundLocationAt(double WorldX_Cm, double WorldY_Cm, float OffsetZ_Cm = 100.f) const;

	/** Chunk Actor collision üretimi bittiðinde burayý bilgilendirir */
	void NotifyChunkFinishedBuilding(FIntPoint Coord);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FTimerHandle StreamingTimerHandle;
	FIntPoint LastPlayerChunkCoord = FIntPoint(INT32_MAX, INT32_MAX);

	UPROPERTY()
	TMap<FIntPoint, TObjectPtr<AWTChunkActor>> ActiveChunks;

	void UpdateStreaming();
	FIntPoint WorldToChunkCoord(const FVector& WorldLocation) const;

	bool bHasPlayerSnappedToGround = false;
	bool bIsTerrainReady = false;
	TSet<FIntPoint> PendingInitialCoords;
};
