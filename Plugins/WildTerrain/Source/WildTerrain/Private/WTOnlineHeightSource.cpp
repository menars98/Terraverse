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

	RecalculateCenterTiles();
}

void UWTOnlineHeightSource::SetLocationByLatLong(double InLat, double InLon)
{
	RegionPreset = EWTWorldRegionPreset::CustomLatLong;
	Latitude = InLat;
	Longitude = InLon;
	RecalculateCenterTiles();
}

void UWTOnlineHeightSource::RecalculateCenterTiles()
{
	const double Rad = FMath::DegreesToRadians(FMath::Clamp(Latitude, -85.0511, 85.0511));
	const int32 N = 1 << ZoomLevel;

	CenterTileX = FMath::FloorToInt((Longitude + 180.0) / 360.0 * N);
	CenterTileY = FMath::FloorToInt((1.0 - FMath::Loge(FMath::Tan(Rad) + (1.0 / FMath::Cos(Rad))) / UE_DOUBLE_PI) / 2.0 * N);
}

bool UWTOnlineHeightSource::IsTileReady(double X, double Y) const
{
	const int32 TileX = CenterTileX + FMath::FloorToInt(X / TileSizeMeters);
	const int32 TileY = CenterTileY + FMath::FloorToInt(Y / TileSizeMeters);

	FScopeLock Lock(&TileLock);
	const FTileData* FoundTile = CachedTiles.Find(FIntPoint(TileX, TileY));
	return FoundTile && FoundTile->bIsReady;
}

float UWTOnlineHeightSource::GetBaseHeightMeters(double X, double Y) const
{
	const int32 TileX = CenterTileX + FMath::FloorToInt(X / TileSizeMeters);
	const int32 TileY = CenterTileY + FMath::FloorToInt(Y / TileSizeMeters);
	const FIntPoint TileCoord(TileX, TileY);

	float RawElevation = 0.f;

	{
		FScopeLock Lock(&TileLock);
		if (const FTileData* FoundTile = CachedTiles.Find(TileCoord))
		{
			if (FoundTile->bIsReady && FoundTile->ElevationData.Num() == 256 * 256)
			{
				const double LocalX = (X - (double(TileX - CenterTileX) * TileSizeMeters)) / TileSizeMeters;
				const double LocalY = (Y - (double(TileY - CenterTileY) * TileSizeMeters)) / TileSizeMeters;

				// --- 1. BILINEAR INTERPOLATION (PÜRÜZSÜZLEÞTÝRME) ---
				// Piksel koordinatýný kesirli (float) olarak hesapla (0..255)
				const float FX = FMath::Clamp(float(LocalX * 255.0), 0.f, 254.99f);
				const float FY = FMath::Clamp(float(LocalY * 255.0), 0.f, 254.99f);

				const int32 X0 = FMath::FloorToInt(FX);
				const int32 Y0 = FMath::FloorToInt(FY);
				const int32 X1 = X0 + 1;
				const int32 Y1 = Y0 + 1;

				const float FracX = FX - float(X0);
				const float FracY = FY - float(Y0);

				// Komþu 4 pikselin yüksekliðini al
				const float H00 = FoundTile->ElevationData[Y0 * 256 + X0];
				const float H10 = FoundTile->ElevationData[Y0 * 256 + X1];
				const float H01 = FoundTile->ElevationData[Y1 * 256 + X0];
				const float H11 = FoundTile->ElevationData[Y1 * 256 + X1];

				// Ýki eksende pürüzsüz ara deðer hesapla (Lego basamaklarýný yok eder)
				const float Top = FMath::Lerp(H00, H10, FracX);
				const float Bottom = FMath::Lerp(H01, H11, FracX);
				RawElevation = FMath::Lerp(Top, Bottom, FracY);
			}
		}
		else
		{
			const_cast<UWTOnlineHeightSource*>(this)->RequestTile(TileX, TileY);
			return 0.f;
		}
	}

	// --- 2. HÝBRÝT ÖLÇEKLEME ---
	float FinalHeight = (RawElevation * ElevationScale) + BaseHeightOffsetMeters;

	// Mikro titreþim (Ýsteðe baðlý)
	if (bAddMicroDetailNoise && MicroDetailWavelengthMeters > 0.f)
	{
		const float MicroNoise = FMath::PerlinNoise2D(FVector2D(X, Y) / MicroDetailWavelengthMeters);
		FinalHeight += MicroNoise * MicroDetailAmplitudeMeters;
	}

	return FinalHeight;
}

void UWTOnlineHeightSource::RequestTile(int32 TileX, int32 TileY)
{
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
	PendingRequests.Remove(TileCoord);

	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		const TArray<uint8>& Payload = Response->GetContent();
		FFileHelper::SaveArrayToFile(Payload, *GetLocalCachePath(TileCoord));
		DecodePNGToElevation(Payload, TileCoord);
	}
	else
	{
		const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
		UE_LOG(LogTemp, Warning, TEXT("Terraverse: Tile [%d, %d] indirilemedi! HTTP Kodu: %d"), TileCoord.X, TileCoord.Y, Code);
	}
}

void UWTOnlineHeightSource::DecodePNGToElevation(const TArray<uint8>& RawPNGData, FIntPoint TileCoord)
{
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

				// Terrarium Elevation Formülü (Metre cinsinden)
				const float Height = (float(R) * 256.f + float(G) + float(B) / 256.f) - 32768.f;
				NewTile.ElevationData[i] = Height;
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