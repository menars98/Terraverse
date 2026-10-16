// Fill out your copyright notice in the Description page of Project Settings.


#include "WTHeightSource.h"

float UWTHeightSource::GetHeightMeters(double X, double Y) const
{
	float H = GetBaseHeightMeters(X, Y);

	if (bCircularBoundary)
	{
		const double R = FMath::Sqrt(X * X + Y * Y);
		const double Inner = WorldRadiusMeters - EdgeFalloffWidthMeters;
		const float T = FMath::Clamp((R - Inner) / EdgeFalloffWidthMeters, 0.0, 1.0);
		const float Smooth = T * T * (3.f - 2.f * T);
		H += Smooth * EdgeRingHeightMeters;
	}
	return H;
}
