// Fill out your copyright notice in the Description page of Project Settings.


#include "WTBiomeConfig.h"
#include "Kismet/KismetMaterialLibrary.h"

void UWTBiomeConfig::ApplyToMaterialCollection(UObject* WorldContextObject, UMaterialParameterCollection* TargetMPC) const
{
	if (!TargetMPC || !WorldContextObject)
	{
		return;
	}

	// Set the colors
	UKismetMaterialLibrary::SetVectorParameterValue(WorldContextObject, TargetMPC, FName("Color_Grass"), GrassColor);
	UKismetMaterialLibrary::SetVectorParameterValue(WorldContextObject, TargetMPC, FName("Color_Rock"), RockColor);
	UKismetMaterialLibrary::SetVectorParameterValue(WorldContextObject, TargetMPC, FName("Color_Snow"), SnowColor);

	// Set the elevation limits
	UKismetMaterialLibrary::SetScalarParameterValue(WorldContextObject, TargetMPC, FName("SnowLineMeters"), SnowLineMeters);
	UKismetMaterialLibrary::SetScalarParameterValue(WorldContextObject, TargetMPC, FName("SlopeThreshold"), SlopeThreshold);
}
