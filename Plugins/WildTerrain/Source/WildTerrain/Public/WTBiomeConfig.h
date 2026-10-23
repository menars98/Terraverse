// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Materials/MaterialParameterCollection.h"
#include "WTBiomeConfig.generated.h"

UENUM(BlueprintType)
enum class EWTBiomeType : uint8
{
	AlpineMeadow  UMETA(DisplayName = "Alp Meadows (Green/Snow)"),
	GrandCanyon   UMETA(DisplayName = "Grand Canyon (Red Rock/Sand)"),
	Icelandic     UMETA(DisplayName = "Icelandic (Black Volcanic/Glacier)"),
	Custom        UMETA(DisplayName = "Custom Biome")
};

UCLASS()
class WILDTERRAIN_API UWTBiomeConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Identity")
	EWTBiomeType BiomeType = EWTBiomeType::AlpineMeadow;

	// --- 1. Material Colors (GPU) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Colors")
	FLinearColor GrassColor = FLinearColor(0.15f, 0.35f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Colors")
	FLinearColor RockColor = FLinearColor(0.18f, 0.18f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Colors")
	FLinearColor SnowColor = FLinearColor(0.9f, 0.95f, 1.0f);

	// --- 2. Elevation and Slope Limits ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Elevation")
	float SnowLineMeters = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Elevation")
	float SlopeThreshold = 0.7f;

	// --- 3. Atmosphere and Fog (Phase 3) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Atmosphere")
	FLinearColor FogColor = FLinearColor(0.6f, 0.7f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Atmosphere")
	float FogDensity = 0.015f;

	// --- 4. Wheel and Driving Physics (Phase 5 Preparation) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Physics")
	float BaseSurfaceFriction = 1.0f;

	/** Instantly applies this biome's settings to the entire world by pressing MPC with a single click */
	UFUNCTION(BlueprintCallable, Category = "Biome")
	void ApplyToMaterialCollection(UObject* WorldContextObject, UMaterialParameterCollection* TargetMPC) const;
};
