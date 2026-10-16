// Fill out your copyright notice in the Description page of Project Settings.


#include "TVChaosVehiclePawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

ATVChaosVehiclePawn::ATVChaosVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = false;

	// Kamera Kolu (Araç arkasýndan takip için optimize edilmiþ ayarlar)
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 650.0f;
	SpringArm->SocketOffset = FVector(0.0f, 0.0f, 150.0f);
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 6.0f;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
}

void ATVChaosVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	// Enhanced Input Context Kaydý
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (VehicleMappingContext)
			{
				Subsystem->AddMappingContext(VehicleMappingContext, 0);
			}
		}
	}

}

void ATVChaosVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (ThrottleAction)
		{
			EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ATVChaosVehiclePawn::ThrottleTriggered);
			EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ATVChaosVehiclePawn::ThrottleCompleted);
		}

		if (SteeringAction)
		{
			EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ATVChaosVehiclePawn::SteeringTriggered);
			EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ATVChaosVehiclePawn::SteeringCompleted);
		}

		if (HandbrakeAction)
		{
			EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ATVChaosVehiclePawn::HandbrakeTriggered);
			EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ATVChaosVehiclePawn::HandbrakeCompleted);
		}

		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATVChaosVehiclePawn::LookTriggered);
		}
	}
}

void ATVChaosVehiclePawn::ThrottleTriggered(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (AxisValue >= 0.0f)
	{
		GetVehicleMovementComponent()->SetThrottleInput(AxisValue);
		GetVehicleMovementComponent()->SetBrakeInput(0.0f);
	}
	else
	{
		GetVehicleMovementComponent()->SetThrottleInput(0.0f);
		GetVehicleMovementComponent()->SetBrakeInput(-AxisValue);
	}
}

void ATVChaosVehiclePawn::ThrottleCompleted(const FInputActionValue& Value)
{
	GetVehicleMovementComponent()->SetThrottleInput(0.0f);
	GetVehicleMovementComponent()->SetBrakeInput(0.0f);
}

void ATVChaosVehiclePawn::SteeringTriggered(const FInputActionValue& Value)
{
	GetVehicleMovementComponent()->SetSteeringInput(Value.Get<float>());
}

void ATVChaosVehiclePawn::SteeringCompleted(const FInputActionValue& Value)
{
	GetVehicleMovementComponent()->SetSteeringInput(0.0f);
}

void ATVChaosVehiclePawn::HandbrakeTriggered(const FInputActionValue& Value)
{
	GetVehicleMovementComponent()->SetHandbrakeInput(true);
	OnHandbrakeStateChanged(true);
}

void ATVChaosVehiclePawn::HandbrakeCompleted(const FInputActionValue& Value)
{
	GetVehicleMovementComponent()->SetHandbrakeInput(false);
	OnHandbrakeStateChanged(false);
}

void ATVChaosVehiclePawn::LookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookAxis = Value.Get<FVector2D>();
	SpringArm->AddRelativeRotation(FRotator(LookAxis.Y, LookAxis.X, 0.0f));
}

float ATVChaosVehiclePawn::GetVehicleSpeedKmh() const
{
	return FMath::Abs(GetVehicleMovementComponent()->GetForwardSpeed()) * 0.036f;
}
