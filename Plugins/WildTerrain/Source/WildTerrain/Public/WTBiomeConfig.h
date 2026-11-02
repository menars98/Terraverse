	#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WTBiomeConfig.generated.h"

class UMaterialParameterCollection;

class UStaticMesh;

USTRUCT(BlueprintType)
struct WILDTERRAIN_API FWTPropRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	FString RuleName = TEXT("Agac veya Kaya");

	/** Buraya istediðin kadar varyasyon (Aðaç 1, Aðaç 2 veya Kaya 1, Kaya 2) koyabilirsin! */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	/** Kaç metrede bir kontrol edilsin? (Örn: Aðaç için 14m, Kaya için 20m) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop", meta = (ClampMin = "4.0"))
	float GridSpacingMeters = 14.f;

	/** O noktada çýkma ihtimali (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance = 0.5f;

	/** Kaç dereceden dik yamaçta çýkamaz? (Aðaçlar için 30°, Kayalar için 60°) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	float MaxSlopeDegrees = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	float MinSlopeDegrees = 0.f;

	/** Kar çizgisine ne kadar kala aðaçlar bitsin? (Aðaç sýnýrý / Tree-line) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	float StopBeforeSnowMeters = 30.f;

	/** Rastgele boyut aralýðý */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	float MinScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	float MaxScale = 1.3f;

	/** True = Kayalar gibi yamaca yapýþsýn. False = Aðaçlar gibi dimdik göðe baksýn! */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	bool bAlignToSurfaceNormal = false;
};

UENUM(BlueprintType)
enum class EWTBiomeType : uint8
{
	AlpineMeadow  UMETA(DisplayName = "Alp Meadows (Green/Snow)"),
	GrandCanyon   UMETA(DisplayName = "Grand Canyon (Red Rock/Sand)"),
	Icelandic     UMETA(DisplayName = "Icelandic (Black Volcanic/Glacier)"),
	Custom        UMETA(DisplayName = "Custom Biome")
};

UCLASS(BlueprintType, Blueprintable)
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
	FLinearColor DirtColor = FLinearColor(0.28f, 0.20f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Colors")
	FLinearColor RockColor = FLinearColor(0.18f, 0.18f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Colors")
	FLinearColor SnowColor = FLinearColor(0.9f, 0.95f, 1.0f);

	// --- 2. Elevation and Slope Limits ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Elevation")
	float SnowLineMeters = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Elevation")
	float SlopeThreshold = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Elevation")
	float SnowBandMeters = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Elevation")
	float SnowJitterMeters = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Elevation")
	float TreeLineGapMeters = 100.f;

	// --- 3. Atmosphere and Fog (Phase 3) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Atmosphere")
	FLinearColor FogColor = FLinearColor(0.6f, 0.7f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Atmosphere")
	float FogDensity = 0.015f;

	// --- 4. Wheel and Driving Physics (Phase 5 Preparation) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Physics")
	float BaseSurfaceFriction = 1.0f;

	// --- 5. Climate and Biome Weighting (Yöntem 2) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdealTemperature = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdealMoisture = 0.5f;

	// Prop Rules
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Biome|Props")
	TArray<FWTPropRule> PropRules;
};