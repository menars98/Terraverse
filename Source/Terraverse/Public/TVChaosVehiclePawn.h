// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "InputActionValue.h"
#include "TVChaosVehiclePawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;

UCLASS(Blueprintable)
class TERRAVERSE_API ATVChaosVehiclePawn : public AWheeledVehiclePawn
{
	GENERATED_BODY()
	
public:
	ATVChaosVehiclePawn();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

	// --- KAMERA ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// --- ENHANCED INPUT ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> VehicleMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SteeringAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HandbrakeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// --- GÝRDÝ METOTLARI ---
	void ThrottleTriggered(const FInputActionValue& Value);
	void ThrottleCompleted(const FInputActionValue& Value);

	void SteeringTriggered(const FInputActionValue& Value);
	void SteeringCompleted(const FInputActionValue& Value);

	void HandbrakeTriggered(const FInputActionValue& Value);
	void HandbrakeCompleted(const FInputActionValue& Value);

	void LookTriggered(const FInputActionValue& Value);

public:
	/** Hýz göstergesi (UI) ve ses efektleri için hýz (km/s) */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Telemetry")
	float GetVehicleSpeedKmh() const;

	/** El freni çekildiðinde Blueprint'te duman veya ses tetiklemek için Event */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle|Events")
	void OnHandbrakeStateChanged(bool bIsActive);
};
