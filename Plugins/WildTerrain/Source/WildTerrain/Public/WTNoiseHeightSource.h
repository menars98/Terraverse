// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WTHeightSource.h"
#include "WTNoiseHeightSource.generated.h"

/**
 * Seeded fBm terrain. A low-frequency "drivability" mask (0..1) scales both relief
 * and detail, so you get plains / hills / rare rough zones instead of uniform noise.
 */
UCLASS()
class WILDTERRAIN_API UWTNoiseHeightSource : public UWTHeightSource
{
	GENERATED_BODY()
	
public:
	UWTNoiseHeightSource();

	UPROPERTY(EditAnywhere, Category = "Noise")
	int32 Seed = 1337;

	/** Large hills. */
	UPROPERTY(EditAnywhere, Category = "Macro", meta = (ClampMin = "0"))
	float MacroAmplitudeMeters = 40.f;

	UPROPERTY(EditAnywhere, Category = "Macro", meta = (ClampMin = "10"))
	float MacroWavelengthMeters = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Macro", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MacroOctaves = 4;

	/** Small bumps. Keep low: high-frequency geometry makes wheels jitter. */
	UPROPERTY(EditAnywhere, Category = "Detail", meta = (ClampMin = "0"))
	float DetailAmplitudeMeters = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Detail", meta = (ClampMin = "5"))
	float DetailWavelengthMeters = 30.f;

	UPROPERTY(EditAnywhere, Category = "Detail", meta = (ClampMin = "1", ClampMax = "4"))
	int32 DetailOctaves = 2;

	/** Very low frequency noise deciding calm vs rough regions. */
	UPROPERTY(EditAnywhere, Category = "Drivability", meta = (ClampMin = "100"))
	float DrivabilityWavelengthMeters = 3000.f;

	/** Relief kept in the calmest regions (0 = totally flat, 1 = no effect). */
	UPROPERTY(EditAnywhere, Category = "Drivability", meta = (ClampMin = "0", ClampMax = "1"))
	float MinReliefInCalmZones = 0.15f;

	/** >1 pushes the mask toward extremes (more plains, rarer mountains). */
	UPROPERTY(EditAnywhere, Category = "Drivability", meta = (ClampMin = "0.1"))
	float DrivabilityContrast = 1.5f;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual float GetBaseHeightMeters(double X, double Y) const override;

private:

	// 
	UPROPERTY(Transient)
	FVector2D CachedNoiseOffset = FVector2D::ZeroVector;

	void UpdateCachedOffset();
};
