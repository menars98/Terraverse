// Fill out your copyright notice in the Description page of Project Settings.


#include "WTChunkManager.h"
#include "WTChunkActor.h"
#include "WTHeightSource.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "WTOnlineHeightSource.h"
#include "WTBiomeConfig.h"
#include "GameFramework/PlayerStart.h"

AWTChunkManager::AWTChunkManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

FVector AWTChunkManager::GetGroundLocationAt(double WorldX_Cm, double WorldY_Cm, float OffsetZ_Cm) const
{
	if (!HeightSource)
	{
		return FVector(WorldX_Cm, WorldY_Cm, OffsetZ_Cm);
	}

	const double XMeters = WorldX_Cm * 0.01;
	const double YMeters = WorldY_Cm * 0.01;
	const float GroundMeters = HeightSource->GetHeightMeters(XMeters, YMeters);

	return FVector(WorldX_Cm, WorldY_Cm, (GroundMeters * 100.0) + OffsetZ_Cm);
}

float AWTChunkManager::GetBiomeWeightAtLocation(FVector WorldLocation) const
{
	const double WX = (WorldLocation.X * 0.01) / 12000.0; // 12 km ölçek
	const double WY = (WorldLocation.Y * 0.01) / 12000.0;

	const double SinVal = FMath::Sin(WX);
	const double CosVal = FMath::Cos(WY);
	const float RawWeight = float((SinVal + CosVal) * 0.5 + 0.5);

	return FMath::Clamp(RawWeight, 0.0f, 1.0f);
}

void AWTChunkManager::NotifyChunkFinishedBuilding(FIntPoint Coord)
{
	if (bIsTerrainReady)
	{
		return;
	}

	PendingInitialCoords.Remove(Coord);

	// Kritik 9 chunk'ýn tamamýnýn fizik ve collision'ý bitti mi?
	if (PendingInitialCoords.IsEmpty())
	{
		bIsTerrainReady = true;

		// GameMode ve dinleyen herkesi haberdar et!
		OnTerrainReady.Broadcast();
	}
}

void AWTChunkManager::BeginPlay()
{
	Super::BeginPlay();

	bIsTerrainReady = false;
	PendingInitialCoords.Empty();

	if (BiomeConfig && BiomeMPC)
	{
		BiomeConfig->ApplyToMaterialCollection(this, BiomeMPC);
	}

	// Baþlangýçta (0,0) etrafýndaki 3x3 = 9 chunk'ý 'Kritik Beklenen' listesine alýyoruz
	for (int32 Y = -1; Y <= 1; ++Y)
	{
		for (int32 X = -1; X <= 1; ++X)
		{
			PendingInitialCoords.Add(FIntPoint(X, Y));
		}
	}

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
	if (!HeightSource)
	{
		return;
	}

	// 1. Merkez noktasýný belirle: Araba varsa arabayý, henüz doðmadýysa PlayerStart'ý referans al!
	FVector CenterLocation = FVector::ZeroVector;
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		CenterLocation = PlayerPawn->GetActorLocation();
	}
	else if (AActor* PlayerStart = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass()))
	{
		CenterLocation = PlayerStart->GetActorLocation();
	}
	else
	{
		CenterLocation = GetActorLocation(); // En kötü ihtimal Manager'ýn kendi konumu
	}

	const FIntPoint CurrentCoord = WorldToChunkCoord(CenterLocation);

	// 2. Gereken chunk koordinatlarýný topla
	TSet<FIntPoint> RequiredCoords;
	for (int32 Y = -ViewDistanceChunks; Y <= ViewDistanceChunks; ++Y)
	{
		for (int32 X = -ViewDistanceChunks; X <= ViewDistanceChunks; ++X)
		{
			RequiredCoords.Add(CurrentCoord + FIntPoint(X, Y));
		}
	}

	// 3. Artýk menzilde olmayanlarý sil
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

	// 4. Mesafeye göre sýrala (Merkez ilk sýrada!)
	TArray<FIntPoint> SortedCoords = RequiredCoords.Array();
	SortedCoords.Sort([&](const FIntPoint& A, const FIntPoint& B)
		{
			const int32 DistA = FMath::Square(A.X - CurrentCoord.X) + FMath::Square(A.Y - CurrentCoord.Y);
			const int32 DistB = FMath::Square(B.X - CurrentCoord.X) + FMath::Square(B.Y - CurrentCoord.Y);
			return DistA < DistB;
		});

	// 5. Chunk üretimi
	UWTOnlineHeightSource* OnlineSource = Cast<UWTOnlineHeightSource>(HeightSource);
	int32 SpawnedThisUpdate = 0;
	const int32 MaxNewChunksPerUpdate = 4;

	for (const FIntPoint& Coord : SortedCoords)
	{
		if (ActiveChunks.Contains(Coord))
		{
			continue;
		}

		if (OnlineSource)
		{
			const double Margin = (2.0 * CellSizeMeters) + (OnlineSource->TileSizeMeters / 128.0);
			const double MinX = double(Coord.X) * ChunkSizeMeters - Margin;
			const double MinY = double(Coord.Y) * ChunkSizeMeters - Margin;
			const double MaxX = double(Coord.X + 1) * ChunkSizeMeters + Margin;
			const double MaxY = double(Coord.Y + 1) * ChunkSizeMeters + Margin;

			if (!OnlineSource->EnsureTilesForRect(MinX, MinY, MaxX, MaxY))
			{
				continue;
			}
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		AWTChunkActor* NewChunk = GetWorld()->SpawnActor<AWTChunkActor>(
			AWTChunkActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		NewChunk->BuildChunkAsync(HeightSource, Coord, ChunkSizeMeters, CellSizeMeters, TerrainMaterial);
		ActiveChunks.Add(Coord, NewChunk);

		SpawnedThisUpdate++;
		if (SpawnedThisUpdate >= MaxNewChunksPerUpdate)
		{
			break;
		}
	}

	LastPlayerChunkCoord = CurrentCoord;
}
