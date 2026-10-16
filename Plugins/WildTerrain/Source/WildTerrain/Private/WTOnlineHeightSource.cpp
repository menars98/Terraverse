// Fill out your copyright notice in the Description page of Project Settings.


#include "WTOnlineHeightSource.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"

UWTOnlineHeightSource::UWTOnlineHeightSource()
{
	UpdatePresetCoordinates();
}

void UWTOnlineHeightSource::PostLoad()
{
	Super::PostLoad();
	UpdatePresetCoordinates();
}

#if WITH_EDITOR
void UWTOnlineHeightSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdatePresetCoordinates();
}
#endif

void UWTOnlineHeightSource::UpdatePresetCoordinates()
{
	switch (RegionPreset)
	{
	case EWTWorldRegionPreset::SwissAlps:
		Latitude = 45.9765; Longitude = 7.7491; // Zermatt / Matterhorn
		break;
	case EWTWorldRegionPreset::GrandCanyon:
		Latitude = 36.1069; Longitude = -112.1129; // Grand Canyon
		break;
	case EWTWorldRegionPreset::Cappadocia:
		Latitude = 38.6431; Longitude = 34.8289; // Kapadokya
		break;
	case EWTWorldRegionPreset::MountFuji:
		Latitude = 35.3606; Longitude = 138.7274; // Fuji Zirvesi
		break;
	case EWTWorldRegionPreset::IcelandHighland:
		Latitude = 64.9312; Longitude = -18.6091; // Izlanda Platosu
		break;
	case EWTWorldRegionPreset::CustomLatLong:
	default:
		break;
	}

	RecalculateMetrics();
}

void UWTOnlineHeightSource::SetLocationByLatLong(double InLat, double InLon)
{
	RegionPreset = EWTWorldRegionPreset::CustomLatLong;
	Latitude = InLat;
	Longitude = InLon;
	RecalculateMetrics();
}

void UWTOnlineHeightSource::RecalculateMetrics()
{
	const double ClampedLat = FMath::Clamp(Latitude, -85.0511, 85.0511);
	const double Rad = FMath::DegreesToRadians(ClampedLat);
	const int32 N = 1 << ZoomLevel;

	// 1. Center Tile hesaplama
	CenterTileX = FMath::FloorToInt((Longitude + 180.0) / 360.0 * N);
	CenterTileY = FMath::FloorToInt((1.0 - FMath::Loge(FMath::Tan(Rad) + (1.0 / FMath::Cos(Rad))) / UE_DOUBLE_PI) / 2.0 * N);

	// 2. Madde 1: Gerçek karo geniþliði ve doðru eðim ölçeklemesi
	CalculatedRealTileWidthMeters = float((40075016.686 * FMath::Cos(Rad)) / double(N));
	if (CalculatedRealTileWidthMeters > 0.0f)
	{
		EffectiveElevationScale = (TileSizeMeters / CalculatedRealTileWidthMeters) * SlopeMultiplier;
	}
	else
	{
		EffectiveElevationScale = 1.0f;
	}
}

bool UWTOnlineHeightSource::EnsureTilesForRect(double MinX, double MinY, double MaxX, double MaxY)
{
	check(IsInGameThread());

	const int32 TX0 = CenterTileX + FMath::FloorToInt(MinX / TileSizeMeters);
	const int32 TX1 = CenterTileX + FMath::FloorToInt(MaxX / TileSizeMeters);
	const int32 TY0 = CenterTileY + FMath::FloorToInt(MinY / TileSizeMeters);
	const int32 TY1 = CenterTileY + FMath::FloorToInt(MaxY / TileSizeMeters);

	bool bAllReady = true;

	for (int32 TY = TY0; TY <= TY1; ++TY)
	{
		for (int32 TX = TX0; TX <= TX1; ++TX)
		{
			bool bReady = false;
			{
				FScopeLock Lock(&TileLock);
				const FTileData* T = CachedTiles.Find(FIntPoint(TX, TY));
				bReady = T && T->bIsReady;
			}

			if (!bReady)
			{
				RequestTile(TX, TY);
				bAllReady = false;
			}
		}
	}

	return bAllReady;
}

// Madde 3: Global Texel Fetch (Tile sýnýrlarýný umursamayan dikiþsiz okuyucu)
float UWTOnlineHeightSource::FetchTexel(int32 GX, int32 GY) const
{
	const int32 TX = FMath::FloorToInt(double(GX) / 256.0);
	const int32 TY = FMath::FloorToInt(double(GY) / 256.0);

	const FTileData* T = CachedTiles.Find(FIntPoint(CenterTileX + TX, CenterTileY + TY));
	if (!T || !T->bIsReady || T->ElevationData.Num() != 256 * 256)
	{
		return 0.f;
	}

	int32 LocalPX = GX - (TX * 256);
	int32 LocalPY = GY - (TY * 256);

	return T->ElevationData[LocalPY * 256 + LocalPX];
}

// Worker Thread'den cagirilir - ASLA HTTP ya da indirme tetiklemez!
float UWTOnlineHeightSource::GetBaseHeightMeters(double X, double Y) const
{
	float RawElevation = 0.f;

	{
		FScopeLock Lock(&TileLock);

		// Madde 3: Texel merkezine hizalanmis dunya orneklemesi
		const double SX = (X / TileSizeMeters) * 256.0 - 0.5;
		const double SY = (Y / TileSizeMeters) * 256.0 - 0.5;

		const int32 X0 = FMath::FloorToInt(SX);
		const int32 Y0 = FMath::FloorToInt(SY);
		const float FracX = float(SX - double(X0));
		const float FracY = float(SY - double(Y0));

		const float H00 = FetchTexel(X0, Y0);
		const float H10 = FetchTexel(X0 + 1, Y0);
		const float H01 = FetchTexel(X0, Y0 + 1);
		const float H11 = FetchTexel(X0 + 1, Y0 + 1);

		const float Top = FMath::Lerp(H00, H10, FracX);
		const float Bottom = FMath::Lerp(H01, H11, FracX);
		RawElevation = FMath::Lerp(Top, Bottom, FracY);
	}

	// Madde 1: Eðim düzeltilmiþ yükseklik hesabý
	float FinalHeight = (RawElevation * EffectiveElevationScale) + BaseHeightOffsetMeters;

	if (bAddMicroDetailNoise && MicroDetailWavelengthMeters > 0.f)
	{
		FinalHeight += FMath::PerlinNoise2D(FVector2D(X, Y) / MicroDetailWavelengthMeters) * MicroDetailAmplitudeMeters;
	}

	return FinalHeight;
}

void UWTOnlineHeightSource::RequestTile(int32 TileX, int32 TileY)
{
	check(IsInGameThread());

	const FIntPoint Coord(TileX, TileY);
	if (PendingRequests.Contains(Coord))
	{
		return;
	}
	PendingRequests.Add(Coord);

	const FString CachePath = GetLocalCachePath(Coord);
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*CachePath))
	{
		TArray<uint8> LocalData;
		if (FFileHelper::LoadFileToArray(LocalData, *CachePath))
		{
			DecodePNGToElevation(LocalData, Coord);
			PendingRequests.Remove(Coord);
			return;
		}
	}

	const FString Url = FString::Printf(TEXT("%s/%d/%d/%d.png"), *BaseUrl, ZoomLevel, TileX, TileY);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindUObject(this, &UWTOnlineHeightSource::OnTileDownloaded, Coord);
	Request->ProcessRequest();
}

void UWTOnlineHeightSource::OnTileDownloaded(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FIntPoint TileCoord)
{
	check(IsInGameThread());
	PendingRequests.Remove(TileCoord);

	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		const TArray<uint8>& Payload = Response->GetContent();
		FFileHelper::SaveArrayToFile(Payload, *GetLocalCachePath(TileCoord));
		DecodePNGToElevation(Payload, TileCoord);
	}
}

void UWTOnlineHeightSource::DecodePNGToElevation(const TArray<uint8>& RawPNGData, FIntPoint TileCoord)
{
	check(IsInGameThread());

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawPNGData.GetData(), RawPNGData.Num()))
	{
		TArray<uint8> UncompressedRGBA;
		if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, UncompressedRGBA))
		{
			FTileData NewTile;
			NewTile.ElevationData.SetNumUninitialized(256 * 256);

			for (int32 i = 0; i < 256 * 256; ++i)
			{
				const uint8 R = UncompressedRGBA[i * 4 + 0];
				const uint8 G = UncompressedRGBA[i * 4 + 1];
				const uint8 B = UncompressedRGBA[i * 4 + 2];
				NewTile.ElevationData[i] = (float(R) * 256.f + float(G) + float(B) / 256.f) - 32768.f;
			}
			NewTile.bIsReady = true;

			FScopeLock Lock(&TileLock);
			CachedTiles.Add(TileCoord, MoveTemp(NewTile));
		}
	}
}

FString UWTOnlineHeightSource::GetLocalCachePath(FIntPoint TileCoord) const
{
	return FPaths::ProjectSavedDir() / TEXT("TerrainCache") / FString::Printf(TEXT("%d_%d_%d.png"), ZoomLevel, TileCoord.X, TileCoord.Y);
}