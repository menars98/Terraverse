#include "WTChunkActor.h"
#include "WTClimate.h"
#include "WTHeightSource.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Async/Async.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "WTBiomeConfig.h"
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

void AWTChunkActor::ClearProps()
{
	for (auto& Pair : PropHISMComponents)
	{
		if (Pair.Value)
		{
			Pair.Value->ClearInstances();
		}
	}
}

FChunkMeshData AWTChunkActor::GenerateRawMeshData(const UWTHeightSource* Source, FIntPoint ChunkCoord,
	float ChunkSizeMeters, float CellSizeMeters, const FWTClimateParams& Climate)
{
	FChunkMeshData Data;
	const int32 Cells = FMath::Max(1, FMath::RoundToInt(ChunkSizeMeters / CellSizeMeters));
	const int32 V = Cells + 1;
	const double OriginX = double(ChunkCoord.X) * ChunkSizeMeters;
	const double OriginY = double(ChunkCoord.Y) * ChunkSizeMeters;

	//Increasing vertices & triangles for better looking, especially for the edges of the chunk. This will add a perimeter of vertices around the chunk.
	const int32 PerimeterCount = Cells * 4;
	Data.Vertices.Reserve((V * V) + PerimeterCount);
	Data.Normals.Reserve((V * V) + PerimeterCount);
	Data.UVs.Reserve((V * V) + PerimeterCount);
	Data.Colors.Reserve((V * V) + PerimeterCount);
	Data.Triangles.Reserve((Cells * Cells * 6) + (PerimeterCount * 6));

	const double E = CellSizeMeters;

	// Grid Vertices
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

	// Grid Triangles
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

	// 3. TERRAIN SKIRTS
	// Vertices and triangles for the skirt (vertical walls) around the chunk to hide gaps between chunks
	const double SkirtDepthCm = 3500.0;

	// Collecting the perimeter corner indices in counter-clockwise (CCW) order
	TArray<int32> PerimeterIndices;
	PerimeterIndices.Reserve(PerimeterCount);

	// Bottom Edge (Y = 0, X: 0 -> Cells)
	for (int32 i = 0; i < Cells; ++i) PerimeterIndices.Add(0 * V + i);
	// Right Edge (X = Cells, Y: 0 -> Cells)
	for (int32 j = 0; j < Cells; ++j) PerimeterIndices.Add(j * V + Cells);
	// Top Edge (Y = Cells, X: Cells -> 0)
	for (int32 i = Cells; i > 0; --i) PerimeterIndices.Add(Cells * V + i);
	// Left Edge (X = 0, Y: Cells -> 0)
	for (int32 j = Cells; j > 0; --j) PerimeterIndices.Add(j * V + 0);

	const int32 NumPerimeter = PerimeterIndices.Num();
	TArray<int32> SkirtIndices;
	SkirtIndices.Reserve(NumPerimeter);

	// A) First, add all bottom skirt vertices with safe local copies (prevents Unreal Engine TArray asserts)
	for (int32 k = 0; k < NumPerimeter; ++k)
	{
		const int32 TopIdx = PerimeterIndices[k];

		// Get local copies (prevents Unreal Engine TArray asserts)
		const FVector TopPos = Data.Vertices[TopIdx];
		const FVector TopNorm = Data.Normals[TopIdx];
		const FVector2D TopUV = Data.UVs[TopIdx];
		const FColor TopCol = Data.Colors[TopIdx];

		const FVector SkirtPos = TopPos - FVector(0.0, 0.0, SkirtDepthCm);

		const int32 BotIdx = Data.Vertices.Add(SkirtPos);
		Data.Normals.Add(TopNorm);
		Data.UVs.Add(TopUV);
		Data.Colors.Add(TopCol);

		SkirtIndices.Add(BotIdx);
	}

	// B) Connect vertical skirt triangles
	for (int32 k = 0; k < NumPerimeter; ++k)
	{
		const int32 Top1 = PerimeterIndices[k];
		const int32 Top2 = PerimeterIndices[(k + 1) % NumPerimeter];
		const int32 Bot1 = SkirtIndices[k];
		const int32 Bot2 = SkirtIndices[(k + 1) % NumPerimeter];

		// Two outward-facing skirt triangles
		Data.Triangles.Add(Top1);
		Data.Triangles.Add(Top2);
		Data.Triangles.Add(Bot1);

		Data.Triangles.Add(Top2);
		Data.Triangles.Add(Bot2);
		Data.Triangles.Add(Bot1);
	}
	return Data;
}

void AWTChunkActor::SpawnPropsForChunk(AWTChunkManager* Manager)
{
	ClearProps();

	if (!Manager || !Manager->HeightSource)
	{
		return;
	}

	const double OriginX = double(Coord.X) * Manager->ChunkSizeMeters;
	const double OriginY = double(Coord.Y) * Manager->ChunkSizeMeters;

	UWTBiomeConfig* Biome = Manager->GetDominantBiomeAtLocation(FVector(OriginX * 100.0, OriginY * 100.0, 0.0));
	if (!Biome || Biome->PropRules.IsEmpty())
	{
		return;
	}

	const int32 ChunkSeed = HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y));
	FRandomStream Rng(ChunkSeed);

	for (const FWTPropRule& Rule : Biome->PropRules)
	{
		if (Rule.Meshes.IsEmpty() || Rule.GridSpacingMeters <= 0.f)
		{
			continue;
		}

		const int32 Steps = FMath::FloorToInt(Manager->ChunkSizeMeters / Rule.GridSpacingMeters);

		for (int32 gy = 0; gy < Steps; ++gy)
		{
			for (int32 gx = 0; gx < Steps; ++gx)
			{
				// Hücre içi rastgele sapma (Jitter)
				const double JitterX = Rng.FRandRange(0.05, 0.95) * Rule.GridSpacingMeters;
				const double JitterY = Rng.FRandRange(0.05, 0.95) * Rule.GridSpacingMeters;

				const double WX = OriginX + (gx * Rule.GridSpacingMeters) + JitterX;
				const double WY = OriginY + (gy * Rule.GridSpacingMeters) + JitterY;

				// --- 1. DOĞAL KÜMELENME (ORMAN AÇIKLIKLARI VE YOLLARI) ---
				// Sadece ağaçlar için kümelenme uyguluyoruz (Kayalar her yerde çıkabilir)
				if (!Rule.bAlignToSurfaceNormal)
				{
					const float ClusterNoise = FMath::PerlinNoise2D(FVector2D(WX, WY) / 180.0f) * 0.5f + 0.5f;
					if (ClusterNoise < 0.42f)
					{
						continue; // Doğal açık çayır alanı bırak!
					}
				}

				// İhtimal kontrolü
				if (Rng.FRand() > Rule.SpawnChance)
				{
					continue;
				}

				const float Height = Manager->HeightSource->GetHeightMeters(WX, WY);

				// Kar sınırında ağaçlar bitsin
				if (Height >= (Biome->SnowLineMeters - Rule.StopBeforeSnowMeters))
				{
					continue;
				}

				// Eğim kontrolü
				const double E = 1.0;
				const float Hx = Manager->HeightSource->GetHeightMeters(WX + E, WY) - Manager->HeightSource->GetHeightMeters(WX - E, WY);
				const float Hy = Manager->HeightSource->GetHeightMeters(WX, WY + E) - Manager->HeightSource->GetHeightMeters(WX, WY - E);
				const FVector Normal = FVector(-Hx / (2.0 * E), -Hy / (2.0 * E), 1.0).GetSafeNormal();

				const float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Normal.Z, -1.f, 1.f)));
				if (SlopeAngle < Rule.MinSlopeDegrees || SlopeAngle > Rule.MaxSlopeDegrees)
				{
					continue;
				}

				const int32 MeshIndex = Rng.RandRange(0, Rule.Meshes.Num() - 1);
				UStaticMesh* SelectedMesh = Rule.Meshes[MeshIndex];
				if (!SelectedMesh) continue;

				// Ağacın kökünün havada kalmaması için 15-20 cm toprağa gömüyoruz
				const float SinkOffsetCm = -18.0f;
				FVector LocalLocation(
					(WX - OriginX) * 100.0,
					(WY - OriginY) * 100.0,
					(Height * 100.0) + SinkOffsetCm
				);

				// --- 2. SENİN İSTEDİĞİN EĞİKLİK (TILT / ROTATION) ---
				FRotator Rotation;
				if (Rule.bAlignToSurfaceNormal)
				{
					// Kayalar yamacın açısına göre yatsın
					Rotation = FRotationMatrix::MakeFromZ(Normal).Rotator();
					Rotation.Yaw += Rng.FRandRange(0.f, 360.f);
				}
				else
				{
					// Ağaçlar: Rastgele Yaw + rüzgar/yamaç eğikliği (Pitch ve Roll)
					const float RandomYaw = Rng.FRandRange(0.f, 360.f);
					const float RandomPitch = Rng.FRandRange(-10.f, 10.f);
					const float RandomRoll = Rng.FRandRange(-10.f, 10.f);
					Rotation = FRotator(RandomPitch, RandomYaw, RandomRoll);
				}

				// --- 3. DAHA GENİŞ BOYUT FARKLILIKLARI ---
				const float Scale = Rng.FRandRange(Rule.MinScale, Rule.MaxScale);

				FTransform InstanceTransform(Rotation, LocalLocation, FVector(Scale));

				TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& HISM = PropHISMComponents.FindOrAdd(SelectedMesh);
				if (!HISM)
				{
					HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
					HISM->SetStaticMesh(SelectedMesh);
					HISM->SetupAttachment(RootComponent);
					HISM->RegisterComponent();
				}

				HISM->AddInstance(InstanceTransform);
			}
		}
	}
}

void AWTChunkActor::ApplyMeshData_GameThread(FChunkMeshData MeshData, UMaterialInterface* Material, bool bEnableCollision)
{
	Mesh->ClearAllMeshSections();
	Mesh->CreateMeshSection(0, MeshData.Vertices, MeshData.Triangles, MeshData.Normals,
		MeshData.UVs, MeshData.Colors, TArray<FProcMeshTangent>(), bEnableCollision);

	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
	bIsBuilding = false;

	// --- FAZ 4: PROP DAĞITIMINI ÇALIŞTIR ---
	if (AWTChunkManager* Manager = Cast<AWTChunkManager>(GetOwner()))
	{
		SpawnPropsForChunk(Manager);
		Manager->NotifyChunkFinishedBuilding(Coord);
	}
}