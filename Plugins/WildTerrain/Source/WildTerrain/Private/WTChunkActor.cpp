#include "WTChunkActor.h"
#include "WTClimate.h"
#include "WTHeightSource.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Async/Async.h"
#include "WTChunkManager.h"

AWTChunkActor::AWTChunkActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->bUseAsyncCooking = true;
	Mesh->bUseComplexAsSimpleCollision = true;
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetRootComponent(Mesh);
}

void AWTChunkActor::BuildChunkAsync(const UWTHeightSource* Source, FIntPoint ChunkCoord, float ChunkSizeMeters,
	float CellSizeMeters, UMaterialInterface* Material, const FWTClimateParams& Climate, bool bEnableCollision)
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

	TWeakObjectPtr<AWTChunkActor> WeakThis(this);

	Async(EAsyncExecution::ThreadPool, [WeakThis, Source, ChunkCoord, ChunkSizeMeters, CellSizeMeters, Material, Climate, bEnableCollision]()
		{
			FChunkMeshData GeneratedData = GenerateRawMeshData(Source, ChunkCoord, ChunkSizeMeters, CellSizeMeters, Climate);

			AsyncTask(ENamedThreads::GameThread, [WeakThis, Data = MoveTemp(GeneratedData), Material, bEnableCollision]() mutable
				{
					if (WeakThis.IsValid())
					{
						WeakThis->ApplyMeshData_GameThread(MoveTemp(Data), Material, bEnableCollision);
					}
				});
		});
}

void AWTChunkActor::BuildChunkSync(const UWTHeightSource* Source, FIntPoint ChunkCoord, float ChunkSizeMeters,
	float CellSizeMeters, UMaterialInterface* Material, const FWTClimateParams& Climate, bool bEnableCollision)
{
	if (!Source || CellSizeMeters <= 0.f || ChunkSizeMeters <= 0.f)
	{
		return;
	}

	Coord = ChunkCoord;
	const double OriginX = double(ChunkCoord.X) * ChunkSizeMeters;
	const double OriginY = double(ChunkCoord.Y) * ChunkSizeMeters;
	SetActorLocation(FVector(OriginX * 100.0, OriginY * 100.0, 0.0));

	FChunkMeshData GeneratedData = GenerateRawMeshData(Source, ChunkCoord, ChunkSizeMeters, CellSizeMeters, Climate);
	ApplyMeshData_GameThread(MoveTemp(GeneratedData), Material, bEnableCollision);
}

FChunkMeshData AWTChunkActor::GenerateRawMeshData(const UWTHeightSource* Source, FIntPoint ChunkCoord,
	float ChunkSizeMeters, float CellSizeMeters, const FWTClimateParams& Climate)
{
	FChunkMeshData Data;
	const int32 Cells = FMath::Max(1, FMath::RoundToInt(ChunkSizeMeters / CellSizeMeters));
	const int32 V = Cells + 1;
	const double OriginX = double(ChunkCoord.X) * ChunkSizeMeters;
	const double OriginY = double(ChunkCoord.Y) * ChunkSizeMeters;

	Data.Vertices.Reserve(V * V);
	Data.Normals.Reserve(V * V);
	Data.UVs.Reserve(V * V);
	Data.Colors.Reserve(V * V);
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

			const FVector2D Clim = Climate.Sample(WX, WY);
			Data.Colors.Add(FColor(uint8(Clim.X * 255.f + 0.5f), uint8(Clim.Y * 255.f + 0.5f), 0, 255));
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

void AWTChunkActor::ApplyMeshData_GameThread(FChunkMeshData MeshData, UMaterialInterface* Material, bool bEnableCollision)
{
	Mesh->ClearAllMeshSections();

	// bEnableCollision false ise fizik pişirme SIFIRDIR (lag yapmaz!)
	Mesh->CreateMeshSection(0, MeshData.Vertices, MeshData.Triangles, MeshData.Normals,
		MeshData.UVs, MeshData.Colors, TArray<FProcMeshTangent>(), bEnableCollision);

	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
	bIsBuilding = false;

	if (AWTChunkManager* Manager = Cast<AWTChunkManager>(GetOwner()))
	{
		Manager->NotifyChunkFinishedBuilding(Coord);
	}
}