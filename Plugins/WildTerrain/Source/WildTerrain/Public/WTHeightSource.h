// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WTHeightSource.generated.h"

/**
 * Height provider. Everything is in METERS, map is centered at (0,0).
 * GetHeightMeters() is const and touches no UObjects at runtime -> safe to call
 * from worker threads (needed for async chunk building in Phase 2).
 *
 * Subclasses override GetBaseHeightMeters(). The (optional) circular boundary is
 * applied here, so noise and heightmap sources share the same world edge behaviour.
 */
UCLASS(Abstract, BlueprintType)
class WILDTERRAIN_API UWTHeightSource : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Finite circular world. Off = infinite. */
	UPROPERTY(EditAnywhere, Category = "Boundary")
	bool bCircularBoundary = false;

	/** Radius where the mountain ring is fully risen. */
	UPROPERTY(EditAnywhere, Category = "Boundary", meta = (EditCondition = "bCircularBoundary", ClampMin = "100"))
	float WorldRadiusMeters = 5000.f;

	/** Ring starts rising this far inside the radius. */
	UPROPERTY(EditAnywhere, Category = "Boundary", meta = (EditCondition = "bCircularBoundary", ClampMin = "10"))
	float EdgeFalloffWidthMeters = 800.f;

	UPROPERTY(EditAnywhere, Category = "Boundary", meta = (EditCondition = "bCircularBoundary"))
	float EdgeRingHeightMeters = 300.f;

	float GetHeightMeters(double X, double Y) const;

protected:
	virtual float GetBaseHeightMeters(double X, double Y) const { return 0.f; }
};
