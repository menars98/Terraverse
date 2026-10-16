// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WTHeightSource.h"
#include "WTFileHeightSource.generated.h"

/**
 * 
 */
UCLASS()
class WILDTERRAIN_API UWTFileHeightSource : public UWTHeightSource
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heightmap")
	TObjectPtr<UTexture2D> HeightmapTexture;

	/** En yüksek noktanýn dünya üzerindeki metre karþýlýðý */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heightmap")
	float MaxElevationMeters = 1500.f;

	/** Haritanýn dünya üzerindeki toplam geniþliði (Metre cinsinden) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heightmap")
	float TerrainWidthMeters = 4096.f;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void CacheTextureData();

protected:
	virtual float GetBaseHeightMeters(double X, double Y) const override;

private:
	UPROPERTY(Transient)
	TArray<float> CachedNormalizedHeights;

	int32 TextureWidth = 0;
	int32 TextureHeight = 0;
};
