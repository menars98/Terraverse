// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WTHeightSource.h"
#include "Interfaces/IHttpRequest.h"
#include "WTOnlineHeightSource.generated.h"

UENUM(BlueprintType)
enum class EWTWorldRegionPreset : uint8
{
	CustomLatLong   UMETA(DisplayName = "Custom Coordinate (Lat / Long)"),
	SwissAlps       UMETA(DisplayName = "Switzerland (Matterhorn)"),
	GrandCanyon     UMETA(DisplayName = "Grand Canyon (USA)"),
	Cappadocia      UMETA(DisplayName = "Cappadocia (Turkey)"),
	MountFuji       UMETA(DisplayName = "Mount Fuji (Japan)"),
	IcelandHighland UMETA(DisplayName = "Iceland Highlands")
};


struct FTileData
{
	TArray<float> ElevationData; // 256x256 float yükseklik
	bool bIsReady = false;
};

UCLASS()
class WILDTERRAIN_API UWTOnlineHeightSource : public UWTHeightSource
{
	GENERATED_BODY()
	
public:
	UWTOnlineHeightSource();

	// --- 1. BOLGE VE KOORDINAT AYARLARI ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	EWTWorldRegionPreset RegionPreset = EWTWorldRegionPreset::SwissAlps;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region", meta = (EditCondition = "RegionPreset == EWTWorldRegionPreset::CustomLatLong"))
	double Latitude = 45.9765;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region", meta = (EditCondition = "RegionPreset == EWTWorldRegionPreset::CustomLatLong"))
	double Longitude = 7.7491;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region", meta = (ClampMin = "5", ClampMax = "12"))
	int32 ZoomLevel = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	float TileSizeMeters = 2000.f;

	// --- 2. EGIM VE OLCEKLEME (Madde 1) ---
	/** 1.0 = Gercek Dunya Egimi, 1.2 - 1.5 = Hafif abartili ve dramatik dag manzarasi */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Scale", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float SlopeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Scale")
	float BaseHeightOffsetMeters = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Scale")
	bool bAddMicroDetailNoise = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Scale", meta = (EditCondition = "bAddMicroDetailNoise"))
	float MicroDetailAmplitudeMeters = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Scale", meta = (EditCondition = "bAddMicroDetailNoise"))
	float MicroDetailWavelengthMeters = 20.f;

	// --- 3. GAME THREAD API (Madde 2) ---
	/** Verilen dunya koordinat dikdortgenini kapsayan tum karolari kontrol eder ve eksikleri Game Thread'de indirir */
	bool EnsureTilesForRect(double MinX, double MinY, double MaxX, double MaxY);

	UFUNCTION(BlueprintCallable, Category = "Terrain|Coordinates")
	void SetLocationByLatLong(double InLat, double InLon);

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** Worker Thread tarafindan cagirilan saf okuma fonksiyonu (Asla indirme yapmaz, kilit minimum tutulur) */
	virtual float GetBaseHeightMeters(double X, double Y) const override;

private:
	int32 CenterTileX = 536;
	int32 CenterTileY = 358;

	UPROPERTY(VisibleAnywhere, Category = "Online|Debug")
	float CalculatedRealTileWidthMeters = 27000.f;

	UPROPERTY(VisibleAnywhere, Category = "Online|Debug")
	float EffectiveElevationScale = 0.074f;

	mutable FCriticalSection TileLock;
	TMap<FIntPoint, FTileData> CachedTiles;

	// SADECE Game Thread tarafindan kullanilir (Kilit gerektirmez)
	TSet<FIntPoint> PendingRequests;

	UPROPERTY(EditAnywhere, Category = "Online")
	FString BaseUrl = TEXT("https://s3.amazonaws.com/elevation-tiles-prod/terrarium");

	void UpdatePresetCoordinates();
	void RecalculateMetrics();
	float FetchTexel(int32 GX, int32 GY) const; // Madde 3: Global Texel Okuyucu
	void RequestTile(int32 TileX, int32 TileY);
	void OnTileDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FIntPoint TileCoord);
	void DecodePNGToElevation(const TArray<uint8>& RawPNGData, FIntPoint TileCoord);
	FString GetLocalCachePath(FIntPoint TileCoord) const;
};
