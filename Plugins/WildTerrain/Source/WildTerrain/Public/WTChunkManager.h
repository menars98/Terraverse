#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WTClimate.h"
#include "WTChunkManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTerrainReadySignature);

class AWTChunkActor;
class UWTHeightSource;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UWTBiomeConfig;
class UMaterialParameterCollection;

UCLASS(Blueprintable)
class WILDTERRAIN_API AWTChunkManager : public AActor
{
	GENERATED_BODY()

public:
	AWTChunkManager();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UWTHeightSource> HeightSource;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Biomes")
	TArray<TObjectPtr<UWTBiomeConfig>> BiomeConfigs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TerrainMID;

	UPROPERTY(BlueprintAssignable, Category = "Terrain|Events")
	FOnTerrainReadySignature OnTerrainReady;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Biome", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float BiomeBlendSharpness = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float ChunkSizeMeters = 256.f;

	// --- LOD AYARLARI (LAG BÝTÝRÝCÝ) ---
	/** Yakýndaki chunk'larýn hücre boyutu (Yüksek detay: 2m) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD")
	float NearCellSizeMeters = 2.f;

	/** Uzaktaki daðlarýn hücre boyutu (Düþük detay: 8m - %80 poligon tasarrufu!) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD")
	float FarCellSizeMeters = 8.f;

	/** Arabanýn etrafýndaki detaylý chunk mesafesi (Collision açýk) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD")
	int32 DetailDistanceChunks = 2;

	/** Ufka kadar görünen hafif silüet daðlarýn mesafesi (Collision KAPALI, 32m hücre!) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD", meta = (ClampMin = "3", ClampMax = "10"))
	int32 HorizonDistanceChunks = 6;

	/** En uzaktaki silüet daðlarýn hücre boyutu (Çok hafif: 32 metre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD")
	float HorizonCellSizeMeters = 32.f;

	/** Arabanýn kaç chunk etrafýnda fiziksel çarpýþma (Collision) olsun? (1 = Arabanýn altý ve 8 komþusu) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|LOD", meta = (ClampMin = "1", ClampMax = "3"))
	int32 CollisionRadiusChunks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	int32 ViewDistanceChunks = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float UpdateInterval = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Climate")
	FWTClimateParams Climate;

	// --- EDITÖR ÖNÝZLEME BUTONLARI (KÖRLÜÐÜ BÝTÝREN KISIM) ---
	/** Editörde Play'e basmadan daðlarý hemen önüne sermek için bas! */
	UFUNCTION(CallInEditor, Category = "Terrain|Editor")
	void GenerateTerrainInEditor();

	/** Editörde oluþturulan önizleme daðlarýný temizlemek için bas */
	UFUNCTION(CallInEditor, Category = "Terrain|Editor")
	void ClearTerrainInEditor();

	/** Editörde etrafa kaç chunk çizileceði */
	UPROPERTY(EditAnywhere, Category = "Terrain|Editor", meta = (ClampMin = "1", ClampMax = "5"))
	int32 EditorPreviewRadius = 2;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terrain")
	bool IsTerrainReady() const { return bIsTerrainReady; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terrain")
	FVector GetGroundLocationAt(double WorldX_Cm, double WorldY_Cm, float OffsetZ_Cm = 250.f) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terrain|Biome")
	UWTBiomeConfig* GetDominantBiomeAtLocation(FVector WorldLocation) const;

	void NotifyChunkFinishedBuilding(FIntPoint Coord);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FTimerHandle StreamingTimerHandle;
	FIntPoint LastPlayerChunkCoord = FIntPoint(INT32_MAX, INT32_MAX);

	UPROPERTY()
	TMap<FIntPoint, TObjectPtr<AWTChunkActor>> ActiveChunks;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RuntimeClimateTexture;

	void GenerateClimateTexture();
	void UpdateStreaming();
	FIntPoint WorldToChunkCoord(const FVector& WorldLocation) const;

	bool bIsTerrainReady = false;
	TSet<FIntPoint> PendingInitialCoords;
};