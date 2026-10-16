// Fill out your copyright notice in the Description page of Project Settings.


#include "WTNoiseHeightSource.h"

namespace
{
	// Normalized fBm, roughly -1..1.
	float FBm(const FVector2D& P, int32 Octaves)
	{
		float Sum = 0.f, Amp = 1.f, Norm = 0.f;
		FVector2D Q = P;
		for (int32 i = 0; i < Octaves; ++i)
		{
			Sum += FMath::PerlinNoise2D(Q) * Amp;
			Norm += Amp;
			Amp *= 0.5f;
			Q *= 2.0;
		}
		return Norm > 0.f ? Sum / Norm : 0.f;
	}
}

UWTNoiseHeightSource::UWTNoiseHeightSource()
{
	UpdateCachedOffset();
}

void UWTNoiseHeightSource::PostLoad()
{
	Super::PostLoad();
	UpdateCachedOffset();
}

#if WITH_EDITOR
void UWTNoiseHeightSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateCachedOffset();
}
#endif

void UWTNoiseHeightSource::UpdateCachedOffset()
{
	FRandomStream Rng(Seed);
	CachedNoiseOffset = FVector2D(Rng.FRandRange(-10000.f, 10000.f), Rng.FRandRange(-10000.f, 10000.f));
}

float UWTNoiseHeightSource::GetBaseHeightMeters(double X, double Y) const
{
	FVector2D P(X, Y);

	// Katmanlarýn birbirine yapýþmasýný önlemek için sabit ofset türetiyoruz (Decorrelation)
	const FVector2D WarpOffset = CachedNoiseOffset + FVector2D(127.1, 311.7);
	const FVector2D MaskOffset = CachedNoiseOffset + FVector2D(269.5, 183.3);
	const FVector2D MacroOffset = CachedNoiseOffset;
	const FVector2D DetailOffset = CachedNoiseOffset + FVector2D(419.2, 371.9);

	// Domain Warping
	const float WarpX = FBm((P + WarpOffset) / 800.f, 2) * 150.f;
	const float WarpY = FBm((P + WarpOffset + FVector2D(41.3, 289.1)) / 800.f, 2) * 150.f;
	P += FVector2D(WarpX, WarpY);

	// Drivability Mask
	float Mask = FBm(P / DrivabilityWavelengthMeters + MaskOffset, 2) * 0.5f + 0.5f;
	Mask = FMath::Pow(FMath::Clamp(Mask, 0.f, 1.f), DrivabilityContrast);
	const float Relief = FMath::Lerp(MinReliefInCalmZones, 1.f, Mask);

	// Macro ve Detail
	const float Macro = FBm(P / MacroWavelengthMeters + MacroOffset, MacroOctaves) * MacroAmplitudeMeters;
	const float Detail = FBm(P / DetailWavelengthMeters + DetailOffset, DetailOctaves) * DetailAmplitudeMeters;

	return (Macro + Detail) * Relief;
}