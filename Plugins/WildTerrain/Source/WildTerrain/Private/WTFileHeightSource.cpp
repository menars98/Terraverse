// Fill out your copyright notice in the Description page of Project Settings.


#include "WTFileHeightSource.h"

#include "WTFileHeightSource.h"

void UWTFileHeightSource::PostLoad()
{
	Super::PostLoad();
	CacheTextureData();
}

#if WITH_EDITOR
void UWTFileHeightSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	CacheTextureData();
}
#endif

void UWTFileHeightSource::CacheTextureData()
{
	if (!HeightmapTexture)
	{
		CachedNormalizedHeights.Empty();
		TextureWidth = TextureHeight = 0;
		return;
	}

#if WITH_EDITOR
	// Editörde texture CPU'ya açýk olmalýdýr (Compression: VectorDisplacement veya Grayscale önerilir)
	HeightmapTexture->ConditionalPostLoad();
	FTexturePlatformData* PlatformData = HeightmapTexture->GetPlatformData();
	if (PlatformData && PlatformData->Mips.Num() > 0)
	{
		FByteBulkData* RawImageData = &PlatformData->Mips[0].BulkData;
		const uint16* RawData = static_cast<const uint16*>(RawImageData->LockReadOnly());
		if (RawData)
		{
			TextureWidth = PlatformData->SizeX;
			TextureHeight = PlatformData->SizeY;
			const int32 TotalPixels = TextureWidth * TextureHeight;

			CachedNormalizedHeights.SetNumUninitialized(TotalPixels);
			for (int32 i = 0; i < TotalPixels; ++i)
			{
				// 16-bit uint (0..65535) deðerini 0.0 - 1.0 arasýna normalize et
				CachedNormalizedHeights[i] = float(RawData[i]) / 65535.f;
			}
			RawImageData->Unlock();
		}
	}
#endif
}

float UWTFileHeightSource::GetBaseHeightMeters(double X, double Y) const
{
	if (CachedNormalizedHeights.IsEmpty() || TextureWidth <= 1 || TextureHeight <= 1)
	{
		return 0.f;
	}

	// Koordinatý (0,0 merkezli kabul ederek) UV 0..1 aralýðýna çevir
	const double HalfWidth = TerrainWidthMeters * 0.5;
	const float U = FMath::Clamp((X + HalfWidth) / TerrainWidthMeters, 0.0, 1.0);
	const float V = FMath::Clamp((Y + HalfWidth) / TerrainWidthMeters, 0.0, 1.0);

	// Bilinear Örnekleme (Pikselleþmeyi engeller)
	const float FX = U * (TextureWidth - 1);
	const float FY = V * (TextureHeight - 1);
	const int32 X0 = FMath::FloorToInt(FX);
	const int32 Y0 = FMath::FloorToInt(FY);
	const int32 X1 = FMath::Min(X0 + 1, TextureWidth - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, TextureHeight - 1);

	const float FracX = FX - X0;
	const float FracY = FY - Y0;

	const float H00 = CachedNormalizedHeights[Y0 * TextureWidth + X0];
	const float H10 = CachedNormalizedHeights[Y0 * TextureWidth + X1];
	const float H01 = CachedNormalizedHeights[Y1 * TextureWidth + X0];
	const float H11 = CachedNormalizedHeights[Y1 * TextureWidth + X1];

	const float Top = FMath::Lerp(H00, H10, FracX);
	const float Bottom = FMath::Lerp(H01, H11, FracX);
	const float FinalNormalizedHeight = FMath::Lerp(Top, Bottom, FracY);

	return FinalNormalizedHeight * MaxElevationMeters;
}