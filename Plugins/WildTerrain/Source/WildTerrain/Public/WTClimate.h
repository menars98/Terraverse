#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "WTClimate.generated.h"

USTRUCT(BlueprintType)
struct WILDTERRAIN_API FWTClimateParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
	int32 Seed = 7;

	/** Biyom yamalarının büyüklüğü (8 km ≈ 15 m/s'de 4-5 dakikada bir değişim) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate", meta = (ClampMin = "500"))
	float WavelengthMeters = 2500.f;

	/** >1: uç iklimlere (çöl, soğuk) daha çok yer açar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate", meta = (ClampMin = "0.5", ClampMax = "2.5"))
	float Contrast = 1.6f;

	// Init() ile hesaplanır, serialize edilmez
	FVector2D OffsetT = FVector2D::ZeroVector;
	FVector2D OffsetM = FVector2D::ZeroVector;

	void Init()
	{
		FRandomStream Rng(Seed);
		OffsetT = FVector2D(Rng.FRandRange(-5000.f, 5000.f), Rng.FRandRange(-5000.f, 5000.f));
		OffsetM = FVector2D(Rng.FRandRange(-5000.f, 5000.f), Rng.FRandRange(-5000.f, 5000.f));
	}

	/** x = sıcaklık, y = nem (0..1). Worker thread'de güvenli. */
	FVector2D Sample(double XMeters, double YMeters) const
	{
		const FVector2D Q = FVector2D(XMeters, YMeters) / WavelengthMeters;
		const float T = FMath::Clamp(0.5f + 0.6f * Contrast * FMath::PerlinNoise2D(Q + OffsetT), 0.f, 1.f);
		const float M = FMath::Clamp(0.5f + 0.6f * Contrast * FMath::PerlinNoise2D(Q + OffsetM), 0.f, 1.f);
		return FVector2D(T, M);
	}
};