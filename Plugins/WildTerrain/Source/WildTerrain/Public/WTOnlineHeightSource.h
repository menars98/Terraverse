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

	// --- 2. SURUS VE OYUN DUZELTMELERI (HIBRI FILTRELER) ---
	/** Dagin dikligini yumusatmak/abartmak icin carpan (Orn: 0.7 daha uysal tepeler yapar) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Tweaks", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float ElevationScale = 0.8f;

	/** En alcak noktayi Z=0 tabanina ceker (Aracin binlerce metre yukarida dogmasini onler) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Tweaks")
	float BaseHeightOffsetMeters = 0.f;

	/** Tekerlek titresimi icin uydu verisinin uzerine mikro cakil gurultusu ekler */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Tweaks")
	bool bAddMicroDetailNoise = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Tweaks", meta = (EditCondition = "bAddMicroDetailNoise"))
	float MicroDetailAmplitudeMeters = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drivability & Tweaks", meta = (EditCondition = "bAddMicroDetailNoise"))
	float MicroDetailWavelengthMeters = 20.f;

	// --- API & CACHE ---
	UPROPERTY(EditAnywhere, Category = "Online")
	FString BaseUrl = TEXT("https://s3.amazonaws.com/elevation-tiles-prod/terrarium");

	UFUNCTION(BlueprintCallable, Category = "Terrain|Coordinates")
	void SetLocationByLatLong(double InLat, double InLon);

	UFUNCTION(BlueprintCallable, Category = "Terrain")
	bool IsTileReady(double X, double Y) const;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual float GetBaseHeightMeters(double X, double Y) const override;

private:
	int32 CenterTileX = 536;
	int32 CenterTileY = 358;

	mutable FCriticalSection TileLock;
	TMap<FIntPoint, FTileData> CachedTiles;
	TSet<FIntPoint> PendingRequests;

	void UpdatePresetCoordinates();
	void RecalculateCenterTiles();
	void RequestTile(int32 TileX, int32 TileY);
	void OnTileDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FIntPoint TileCoord);
	void DecodePNGToElevation(const TArray<uint8>& RawPNGData, FIntPoint TileCoord);
	FString GetLocalCachePath(FIntPoint TileCoord) const;
};
