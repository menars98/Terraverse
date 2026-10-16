// Fill out your copyright notice in the Description page of Project Settings.


#include "WTChunkActor.h"
#include "WTHeightSource.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Async/Async.h"
#include <WTChunkManager.h>

AWTChunkActor::AWTChunkActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->bUseAsyncCooking = true;
	Mesh->bUseComplexAsSimpleCollision = true; // trimesh collision for wheels/raycasts
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetRootComponent(Mesh);

}

void AWTChunkActor::BuildChunkAsync(const UWTHeightSource* Source, FIntPoint ChunkCoord, float ChunkSizeMeters, float CellSizeMeters, UMaterialInterface* Material)
{
	if (!Source || CellSizeMeters <= 0.f || ChunkSizeMeters <= 0.f || bIsBuilding)
	{
		return;
	}

	Coord = ChunkCoord;
	bIsBuilding = true;

	const double OriginX = double(ChunkCoord.X) * ChunkSizeMeters;
	const double OriginY = double(ChunkCoord.Y) * ChunkSizeMeters;
	SetActorLocation(FVector(OriginX * 100.0, OriginY * 100.0, 0.0));

	// TWeakObjectPtr ile Actor iþlem sürerken silinirse (Destroy) çökmesini engelliyoruz
	TWeakObjectPtr<AWTChunkActor> WeakThis(this);

	// Arka plandaki Worker Thread'e iþi pasla
	Async(EAsyncExecution::ThreadPool, [WeakThis, Source, ChunkCoord, ChunkSizeMeters, CellSizeMeters, Material]()
	{
		FChunkMeshData GeneratedData = GenerateRawMeshData(Source, ChunkCoord, ChunkSizeMeters, CellSizeMeters);

		// 2. Ýþ bitince sonucu GameThread'e güvenle teslim et
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Data = MoveTemp(GeneratedData), Material]() mutable
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ApplyMeshData_GameThread(MoveTemp(Data), Material);
			}
		});
	});
}

FChunkMeshData AWTChunkActor::GenerateRawMeshData(const UWTHeightSource* Source, FIntPoint ChunkCoord,
	float ChunkSizeMeters, float CellSizeMeters)
{
	FChunkMeshData Data;
	const int32 Cells = FMath::Max(1, FMath::RoundToInt(ChunkSizeMeters / CellSizeMeters));
	const int32 V = Cells + 1;
	const double OriginX = double(ChunkCoord.X) * ChunkSizeMeters;
	const double OriginY = double(ChunkCoord.Y) * ChunkSizeMeters;

	Data.Vertices.Reserve(V * V);
	Data.Normals.Reserve(V * V);
	Data.UVs.Reserve(V * V);
	Data.Triangles.Reserve(Cells * Cells * 6);

	const double E = CellSizeMeters;

	for (int32 j = 0; j < V; ++j)
	{
		for (int32 i = 0; i < V; ++i)
		{
			const double WX = OriginX + double(i) * CellSizeMeters;
			const double WY = OriginY + double(j) * CellSizeMeters;
			const float H = Source->GetHeightMeters(WX, WY);

			Data.Vertices.Add(FVector(i * CellSizeMeters * 100.0, j * CellSizeMeters * 100.0, H * 100.0));

			const float Hx = Source->GetHeightMeters(WX + E, WY) - Source->GetHeightMeters(WX - E, WY);
			const float Hy = Source->GetHeightMeters(WX, WY + E) - Source->GetHeightMeters(WX, WY - E);
			Data.Normals.Add(FVector(-Hx / (2.0 * E), -Hy / (2.0 * E), 1.0).GetSafeNormal());

			Data.UVs.Add(FVector2D(WX * 0.1, WY * 0.1));
		}
	}

	for (int32 j = 0; j < Cells; ++j)
	{
		for (int32 i = 0; i < Cells; ++i)
		{
			const int32 A = j * V + i;
			const int32 B = A + 1;
			const int32 C = A + V;
			const int32 D = C + 1;
			Data.Triangles.Add(A); Data.Triangles.Add(C); Data.Triangles.Add(B);
			Data.Triangles.Add(B); Data.Triangles.Add(C); Data.Triangles.Add(D);
		}
	}

	return Data;
}

void AWTChunkActor::ApplyMeshData_GameThread(FChunkMeshData MeshData, UMaterialInterface* Material)
{
	Mesh->ClearAllMeshSections();
	Mesh->CreateMeshSection(0, MeshData.Vertices, MeshData.Triangles, MeshData.Normals, MeshData.UVs,
		TArray<FColor>(), TArray<FProcMeshTangent>(), /*bCreateCollision=*/true);

	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
	bIsBuilding = false;

	// Manager'a "Benim collision'ým ve mesh'im hazýr!" haberi veriyoruz:
	if (AWTChunkManager* Manager = Cast<AWTChunkManager>(GetOwner()))
	{
		Manager->NotifyChunkFinishedBuilding(Coord);
	}
}