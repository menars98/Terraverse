// Fill out your copyright notice in the Description page of Project Settings.


#include "WTChunkTestSpawner.h"
#include "WTChunkActor.h"
#include "WTHeightSource.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "WTOnlineHeightSource.h"

AWTChunkManager::AWTChunkManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWTChunkManager::BeginPlay()
{
	Super::BeginPlay();

	// Tick yerine Timer baþlat
	GetWorldTimerManager().SetTimer(StreamingTimerHandle, this, &AWTChunkManager::UpdateStreaming, UpdateInterval, true, 0.0f);
}

void AWTChunkManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StreamingTimerHandle);
	Super::EndPlay(EndPlayReason);
}

FIntPoint AWTChunkManager::WorldToChunkCoord(const FVector& WorldLocation) const
{
	const double ChunkSizeCm = ChunkSizeMeters * 100.0;
	return FIntPoint(
		FMath::FloorToInt(WorldLocation.X / ChunkSizeCm),
		FMath::FloorToInt(WorldLocation.Y / ChunkSizeCm)
	);
}

void AWTChunkManager::UpdateStreaming()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn || !HeightSource)
	{
		return;
	}

	// --- 1. OYUNCUYU OTOMATIK ZEMINE OTURTMA (GROUND SNAPPING) ---
	if (bAutoSnapPlayerToGround && !bHasPlayerSnappedToGround)
	{
		const FVector PlayerLoc = PlayerPawn->GetActorLocation();
		const double WorldXMeters = PlayerLoc.X * 0.01;
		const double WorldYMeters = PlayerLoc.Y * 0.01;

		UWTOnlineHeightSource* OnlineSource = Cast<UWTOnlineHeightSource>(HeightSource);
		const bool bIsReady = OnlineSource ? OnlineSource->IsTileReady(WorldXMeters, WorldYMeters) : true;

		if (bIsReady)
		{
			// Zeminin yuksekligini tam olarak al
			const float GroundHeightMeters = HeightSource->GetHeightMeters(WorldXMeters, WorldYMeters);
			const double TargetZ_Cm = (GroundHeightMeters * 100.0) + GroundSpawnOffsetCm;

			FVector NewLoc = PlayerLoc;
			NewLoc.Z = TargetZ_Cm;

			// Araci teleport et ve varsa dusme hizini sifirla
			PlayerPawn->SetActorLocation(NewLoc, false, nullptr, ETeleportType::ResetPhysics);

			// Eger aracýn govdesinde Physics simulasyonu varsa hizi durdur
			if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(PlayerPawn->GetRootComponent()))
			{
				RootPrim->SetPhysicsLinearVelocity(FVector::ZeroVector);
			}

			bHasPlayerSnappedToGround = true;
			UE_LOG(LogTemp, Log, TEXT("Terraverse: Oyuncu basariyla zemine oturtuldu! Z: %.1f cm"), TargetZ_Cm);
		}
		else if (OnlineSource)
		{
			// Henuz karo inmediyse indirmeyi tetikle ve aracin asagi dusmesini engellemek icin hizi sifirla
			OnlineSource->GetHeightMeters(WorldXMeters, WorldYMeters);
			if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(PlayerPawn->GetRootComponent()))
			{
				RootPrim->SetPhysicsLinearVelocity(FVector::ZeroVector);
			}
		}
	}

	const FIntPoint CurrentCoord = WorldToChunkCoord(PlayerPawn->GetActorLocation());

	TSet<FIntPoint> RequiredCoords;
	for (int32 Y = -ViewDistanceChunks; Y <= ViewDistanceChunks; ++Y)
	{
		for (int32 X = -ViewDistanceChunks; X <= ViewDistanceChunks; ++X)
		{
			RequiredCoords.Add(CurrentCoord + FIntPoint(X, Y));
		}
	}

	// 1. Artýk menzilde olmayan chunk'larý sil
	TArray<FIntPoint> CoordsToRemove;
	for (auto& Pair : ActiveChunks)
	{
		if (!RequiredCoords.Contains(Pair.Key))
		{
			if (Pair.Value)
			{
				Pair.Value->Destroy();
			}
			CoordsToRemove.Add(Pair.Key);
		}
	}
	for (const FIntPoint& Coord : CoordsToRemove)
	{
		ActiveChunks.Remove(Coord);
	}

	// 2. Yeni gereken chunk'larý asenkron spawn et (Döngü burada baþlar)
	UWTOnlineHeightSource* OnlineSource = Cast<UWTOnlineHeightSource>(HeightSource);

	for (const FIntPoint& Coord : RequiredCoords)
	{
		if (!ActiveChunks.Contains(Coord))
		{
			// EÐER ONLINE KAYNAK KULLANILIYORSA VE TILE HENÜZ ÝNMEDÝYSE BEKLE
			if (OnlineSource)
			{
				const double CenterX = (double(Coord.X) + 0.5) * ChunkSizeMeters;
				const double CenterY = (double(Coord.Y) + 0.5) * ChunkSizeMeters;

				if (!OnlineSource->IsTileReady(CenterX, CenterY))
				{
					// Ýstek at (download baþlat) ve bu frame bu chunk'ý atla (döngüdeki sonrakine geç)
					OnlineSource->GetBaseHeightMeters(CenterX, CenterY);
					continue; // Artýk bir döngünün içinde olduðu için geçerlidir!
				}
			}

			// Karo hazýrsa chunk aktörünü spawn et
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			AWTChunkActor* NewChunk = GetWorld()->SpawnActor<AWTChunkActor>(
				AWTChunkActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

			NewChunk->BuildChunkAsync(HeightSource, Coord, ChunkSizeMeters, CellSizeMeters, TerrainMaterial);
			ActiveChunks.Add(Coord, NewChunk);
		}
	}

	LastPlayerChunkCoord = CurrentCoord;
}
