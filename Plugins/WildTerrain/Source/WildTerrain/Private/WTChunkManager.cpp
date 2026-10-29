#include "WTChunkManager.h"
#include "WTChunkActor.h"
#include "WTHeightSource.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "WTOnlineHeightSource.h"
#include "WTBiomeConfig.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"

AWTChunkManager::AWTChunkManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWTChunkManager::GenerateTerrainInEditor()
{
	ClearTerrainInEditor();

	if (!HeightSource)
	{
		UE_LOG(LogTemp, Warning, TEXT("Terraverse: HeightSource atanmamis!"));
		return;
	}

	Climate.Init();

	if (TerrainMaterial && !TerrainMID)
	{
		TerrainMID = UMaterialInstanceDynamic::Create(TerrainMaterial, this);
	}
	GenerateClimateTexture();

	// Manager aktörünün koordinatını merkez al
	const FIntPoint CenterCoord = WorldToChunkCoord(GetActorLocation());

	// --- İŞTE EKSİK OLAN KISIM: DİSKTEKİ KAROLARI RAM'E YÜKLE ---
	if (UWTOnlineHeightSource* OnlineSource = Cast<UWTOnlineHeightSource>(HeightSource))
	{
		const double TotalRadiusMeters = (HorizonDistanceChunks + 1) * ChunkSizeMeters;
		const double CenterX = GetActorLocation().X * 0.01;
		const double CenterY = GetActorLocation().Y * 0.01;

		// Diskten karoları hafızaya yükle
		OnlineSource->EnsureTilesForRect(
			CenterX - TotalRadiusMeters,
			CenterY - TotalRadiusMeters,
			CenterX + TotalRadiusMeters,
			CenterY + TotalRadiusMeters
		);
	}

	// --- ŞİMDİ CHUNK'LARI OLUŞTUR (Artık yükseklikleri okuyabilecek!) ---
	for (int32 Y = -HorizonDistanceChunks; Y <= HorizonDistanceChunks; ++Y)
	{
		for (int32 X = -HorizonDistanceChunks; X <= HorizonDistanceChunks; ++X)
		{
			const FIntPoint Coord = CenterCoord + FIntPoint(X, Y);
			const int32 Dist = FMath::Max(FMath::Abs(X), FMath::Abs(Y));

			const bool bHasCollision = (Dist <= CollisionRadiusChunks);

			// 3 Kademeli Akıllı LOD Hesabı (Editörün donmasını engeller):
			float CellSize = NearCellSizeMeters;
			if (Dist > DetailDistanceChunks)
			{
				CellSize = HorizonCellSizeMeters; // 32m (Ufuktaki dağlar - Ultra hafif!)
			}
			else if (Dist > 1)
			{
				CellSize = FarCellSizeMeters; // 8m (Orta mesafe)
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			AWTChunkActor* NewChunk = GetWorld()->SpawnActor<AWTChunkActor>(
				AWTChunkActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

			NewChunk->BuildChunkSync(HeightSource, Coord, ChunkSizeMeters, CellSize,
				TerrainMID ? TerrainMID : TerrainMaterial, Climate, bHasCollision);

			ActiveChunks.Add(Coord, NewChunk);
		}
	}
}

void AWTChunkManager::ClearTerrainInEditor()
{
	for (auto& Pair : ActiveChunks)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	ActiveChunks.Empty();
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

UWTBiomeConfig* AWTChunkManager::GetDominantBiomeAtLocation(FVector WorldLocation) const
{
	if (BiomeConfigs.IsEmpty())
	{
		return nullptr;
	}

	const FVector2D C = Climate.Sample(WorldLocation.X * 0.01, WorldLocation.Y * 0.01);
	const float CurrentTemp = C.X;
	const float CurrentMoisture = C.Y;

	UWTBiomeConfig* BestBiome = BiomeConfigs[0];
	float ShortestDistSq = FLT_MAX;

	for (UWTBiomeConfig* Biome : BiomeConfigs)
	{
		if (!Biome) continue;

		const float DistSq = FMath::Square(CurrentTemp - Biome->IdealTemperature) +
			FMath::Square(CurrentMoisture - Biome->IdealMoisture);

		if (DistSq < ShortestDistSq)
		{
			ShortestDistSq = DistSq;
			BestBiome = Biome;
		}
	}

	return BestBiome;
}

void AWTChunkManager::NotifyChunkFinishedBuilding(FIntPoint Coord)
{
	if (bIsTerrainReady)
	{
		return;
	}

	PendingInitialCoords.Remove(Coord);

	if (PendingInitialCoords.IsEmpty())
	{
		bIsTerrainReady = true;
		OnTerrainReady.Broadcast();
	}
}

void AWTChunkManager::BeginPlay()
{
	// Editörden kalma eski önizleme chunk'ları varsa temizle
	ClearTerrainInEditor();

	Climate.Init();

	if (TerrainMaterial)
	{
		TerrainMID = UMaterialInstanceDynamic::Create(TerrainMaterial, this);
		GenerateClimateTexture();
	}

	Super::BeginPlay();

	bIsTerrainReady = false;
	PendingInitialCoords.Empty();

	FVector InitialCenter = FVector::ZeroVector;
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		InitialCenter = PlayerPawn->GetActorLocation();
	}
	else if (AActor* PlayerStart = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass()))
	{
		InitialCenter = PlayerStart->GetActorLocation();
	}
	else
	{
		InitialCenter = GetActorLocation();
	}

	const FIntPoint StartCoord = WorldToChunkCoord(InitialCenter);
	for (int32 Y = -1; Y <= 1; ++Y)
	{
		for (int32 X = -1; X <= 1; ++X)
		{
			PendingInitialCoords.Add(StartCoord + FIntPoint(X, Y));
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

void AWTChunkManager::GenerateClimateTexture()
{
	if (!TerrainMID || BiomeConfigs.IsEmpty())
	{
		return;
	}

	const int32 Width = 256;
	const int32 Height = 64;

	RuntimeClimateTexture = UTexture2D::CreateTransient(Width, Height, PF_FloatRGBA);
	RuntimeClimateTexture->Filter = TF_Bilinear;
	RuntimeClimateTexture->SRGB = 0;
	RuntimeClimateTexture->AddressX = TA_Clamp;
	RuntimeClimateTexture->AddressY = TA_Clamp;

	FByteBulkData* RawImageData = &RuntimeClimateTexture->GetPlatformData()->Mips[0].BulkData;
	FFloat16Color* Pixels = static_cast<FFloat16Color*>(RawImageData->Lock(LOCK_READ_WRITE));

	for (int32 Y = 0; Y < Height; ++Y)
	{
		const float M = float(Y) / float(Height - 1);

		for (int32 X = 0; X < Width; ++X)
		{
			const int32 SliceIndex = X / 64;
			const float T = float(X % 64) / 63.f; // Heat

			FLinearColor BlendedColor = FLinearColor::Black;
			float TotalWeight = 0.f;

			// En yakın biyomu bulmak için (Kar sınırının bozulmaması için)
			const UWTBiomeConfig* ClosestBiome = nullptr;
			float ShortestDistSq = FLT_MAX;

			for (const UWTBiomeConfig* Biome : BiomeConfigs)
			{
				if (!Biome) continue;

				const float DistSq = FMath::Square(T - Biome->IdealTemperature) + FMath::Square(M - Biome->IdealMoisture);

				// 1. En yakın biyomu takip et (Alpha kanalları için)
				if (DistSq < ShortestDistSq)
				{
					ShortestDistSq = DistSq;
					ClosestBiome = Biome;
				}

				// 2. Renkler için yumuşak harmanlama ağırlığı
				const float Weight = 1.0f / (FMath::Pow(DistSq, BiomeBlendSharpness) + 1e-6f);

				FLinearColor TargetColor;
				switch (SliceIndex)
				{
				case 0:  TargetColor = Biome->GrassColor; break;
				case 1:  TargetColor = Biome->DirtColor;  break;
				case 2:  TargetColor = Biome->RockColor;  break;
				default: TargetColor = Biome->SnowColor;  break;
				}

				BlendedColor += TargetColor * Weight;
				TotalWeight += Weight;
			}

			if (TotalWeight > 0.f)
			{
				BlendedColor /= TotalWeight;
			}

			// KRİTİK NOKTA: Alpha değerleri ortalama ALINMAZ! Doğrudan o pikselin ait olduğu biyomdan net alınır:
			float FinalAlpha = 0.f;
			if (ClosestBiome)
			{
				switch (SliceIndex)
				{
				case 0:  FinalAlpha = ClosestBiome->SnowLineMeters;    break;
				case 1:  FinalAlpha = ClosestBiome->SnowBandMeters;    break;
				case 2:  FinalAlpha = ClosestBiome->SnowJitterMeters;  break;
				default: FinalAlpha = ClosestBiome->TreeLineGapMeters; break;
				}
			}

			Pixels[Y * Width + X] = FFloat16Color(FLinearColor(BlendedColor.R, BlendedColor.G, BlendedColor.B, FinalAlpha));
		}
	}

	RawImageData->Unlock();
	RuntimeClimateTexture->UpdateResource();

	TerrainMID->SetTextureParameterValue(FName("ClimateLUT"), RuntimeClimateTexture);
}

void AWTChunkManager::UpdateStreaming()
{
	if (!HeightSource)
	{
		return;
	}

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
		CenterLocation = GetActorLocation();
	}

	const FIntPoint CurrentCoord = WorldToChunkCoord(CenterLocation);

	// 1. Ufka kadar olan tüm koordinatları topla (HorizonDistanceChunks kadar!)
	TSet<FIntPoint> RequiredCoords;
	for (int32 Y = -HorizonDistanceChunks; Y <= HorizonDistanceChunks; ++Y)
	{
		for (int32 X = -HorizonDistanceChunks; X <= HorizonDistanceChunks; ++X)
		{
			RequiredCoords.Add(CurrentCoord + FIntPoint(X, Y));
		}
	}

	// 2. Artık menzilde olmayanları sil
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

	// 3. Mesafeye göre sırala (Merkez ilk sırada)
	TArray<FIntPoint> SortedCoords = RequiredCoords.Array();
	SortedCoords.Sort([&](const FIntPoint& A, const FIntPoint& B)
		{
			const int32 DistA = FMath::Square(A.X - CurrentCoord.X) + FMath::Square(A.Y - CurrentCoord.Y);
			const int32 DistB = FMath::Square(B.X - CurrentCoord.X) + FMath::Square(B.Y - CurrentCoord.Y);
			return DistA < DistB;
		});

	UWTOnlineHeightSource* OnlineSource = Cast<UWTOnlineHeightSource>(HeightSource);
	int32 SpawnedThisUpdate = 0;
	const int32 MaxNewChunksPerUpdate = 3;

	for (const FIntPoint& Coord : SortedCoords)
	{
		if (ActiveChunks.Contains(Coord))
		{
			continue;
		}

		// --- KADEMELİ LOD VE COLLISION SİSTEMİ ---
		const int32 DistFromPlayer = FMath::Max(FMath::Abs(Coord.X - CurrentCoord.X), FMath::Abs(Coord.Y - CurrentCoord.Y));

		// Sadece yakındaki chunk'lar fizik çarpışması (Collision) üretir!
		const bool bNeedsCollision = (DistFromPlayer <= CollisionRadiusChunks);

		// 3 Kademeli Hücre Boyutu:
		float CellSize = NearCellSizeMeters; // 2m (Arabanın altı)
		if (DistFromPlayer > DetailDistanceChunks)
		{
			CellSize = HorizonCellSizeMeters; // 32m (Ufuktaki dağlar - Sıfır maliyet!)
		}
		else if (DistFromPlayer > 1)
		{
			CellSize = FarCellSizeMeters; // 8m (Orta mesafe)
		}

		if (OnlineSource)
		{
			const double Margin = (2.0 * CellSize) + (OnlineSource->TileSizeMeters / 128.0);
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

		NewChunk->BuildChunkAsync(HeightSource, Coord, ChunkSizeMeters, CellSize,
			TerrainMID ? TerrainMID : TerrainMaterial, Climate, bNeedsCollision);

		ActiveChunks.Add(Coord, NewChunk);

		SpawnedThisUpdate++;
		if (SpawnedThisUpdate >= MaxNewChunksPerUpdate)
		{
			break;
		}
	}

	LastPlayerChunkCoord = CurrentCoord;
}