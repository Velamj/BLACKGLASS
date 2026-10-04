#include "BlackglassGame.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Texture.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace {
AStaticMeshActor* Block(UWorld* World, const FVector& Position, const FVector& Size,
    const FLinearColor& Color, bool Collision = true, const FRotator& Rotation = FRotator::ZeroRotator, bool Asphalt = false) {
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Position, Rotation);
    if (!Actor) return nullptr;
    UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Static);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Actor->SetActorScale3D(Size / 100.f);
    Mesh->SetCollisionProfileName(Collision ? TEXT("BlockAll") : TEXT("NoCollision"));
    Mesh->SetCanEverAffectNavigation(Collision);
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_BGIndustrialSurface.M_BGIndustrialSurface"))) {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Actor);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetScalarParameterValue(TEXT("Roughness"), Asphalt ? .94f : .84f);
        Material->SetScalarParameterValue(TEXT("DetailScale"), FMath::Clamp(static_cast<float>(FMath::Max(Size.X, Size.Y) / 180.), 1.f, 70.f));
        if (Asphalt) {
            Material->SetTextureParameterValue(TEXT("SurfaceDetail"),
                LoadObject<UTexture>(nullptr, TEXT("/Game/Textures/T_BG_AsphaltWear.T_BG_AsphaltWear")));
            Material->SetTextureParameterValue(TEXT("SurfaceNormal"),
                LoadObject<UTexture>(nullptr, TEXT("/Game/Textures/T_BG_AsphaltWearNormal.T_BG_AsphaltWearNormal")));
        }
        Mesh->SetMaterial(0, Material);
    }
    Actor->Tags.Add(TEXT("BlackglassDistrict"));
    return Actor;
}

// Static visual attachments share material batches; they never supply cover or navigation.
struct FBlackglassDressing {
    AActor* Owner = nullptr;
    UStaticMesh* Cube = nullptr;
    UStaticMesh* Cylinder = nullptr;
    UMaterialInterface* Surface = nullptr;
    TMap<FName, UInstancedStaticMeshComponent*> Batches;
    int32 Instances = 0;
    explicit FBlackglassDressing(UWorld* World) {
        Owner = World->SpawnActor<AActor>();
        if (!Owner) return;
        Owner->Tags.Add(TEXT("BlackglassVisualDressing"));
        USceneComponent* Root = NewObject<USceneComponent>(Owner, TEXT("VisualAttachmentsRoot"));
        Root->SetMobility(EComponentMobility::Static);
        Owner->SetRootComponent(Root);
        Owner->AddInstanceComponent(Root);
        Root->RegisterComponent();
        Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_BGIndustrialSurface.M_BGIndustrialSurface"));
    }
    void Add(FName Key, const FVector& Position, const FVector& Size, const FLinearColor& Color,
        float Roughness = .8f, float DetailStrength = 0.f, FRotator Rotation = FRotator::ZeroRotator,
        float Metalness = 0.f, float Emission = 0.f, bool UseCylinder = false) {
        UStaticMesh* Shape = UseCylinder ? Cylinder : Cube;
        if (!Owner || !Shape || !Surface) return;
        UInstancedStaticMeshComponent* Batch = Batches.FindRef(Key);
        if (!Batch) {
            Batch = NewObject<UInstancedStaticMeshComponent>(Owner, Key);
            Owner->AddInstanceComponent(Batch);
            Batch->SetupAttachment(Owner->GetRootComponent());
            Batch->SetMobility(EComponentMobility::Static);
            Batch->SetStaticMesh(Shape);
            Batch->SetCollisionProfileName(TEXT("NoCollision"));
            Batch->SetCanEverAffectNavigation(false);
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Surface, Owner);
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
            Material->SetScalarParameterValue(TEXT("DetailStrength"), DetailStrength);
            Material->SetScalarParameterValue(TEXT("DetailScale"), 2.f);
            Material->SetScalarParameterValue(TEXT("Metalness"), Metalness);
            Material->SetScalarParameterValue(TEXT("Emission"), Emission);
            Batch->SetMaterial(0, Material);
            Batch->RegisterComponent();
            Batches.Add(Key, Batch);
        }
        Batch->AddInstance(FTransform(Rotation.Quaternion(), Position, Size / 100.f), true);
        ++Instances;
    }
    void Pipe(FName Key, const FVector& Start, const FVector& End, float Diameter,
        const FLinearColor& Color, float Metalness = .55f) {
        const FVector Direction = End - Start;
        const float Length = Direction.Size();
        if (Length <= KINDA_SMALL_NUMBER) return;
        const FRotator Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Direction / Length).Rotator();
        Add(Key, (Start + End) * .5f, FVector(Diameter, Diameter, Length), Color, .53f, .12f,
            Rotation, Metalness, 0.f, true);
    }
    void Label(const FString& Text, const FVector& Position, const FRotator& Facing, float Height = 30.f) {
        if (!Owner) return;
        UTextRenderComponent* Sign = NewObject<UTextRenderComponent>(Owner);
        Owner->AddInstanceComponent(Sign);
        Sign->SetupAttachment(Owner->GetRootComponent());
        Sign->SetMobility(EComponentMobility::Static);
        Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Sign->SetCanEverAffectNavigation(false);
        Sign->SetText(FText::FromString(Text));
        Sign->SetTextRenderColor(FColor(218, 214, 187));
        Sign->SetHorizontalAlignment(EHTA_Center);
        Sign->SetVerticalAlignment(EVRTA_TextCenter);
        Sign->SetWorldSize(Height);
        // Establish the static sign transform before registration; runtime moves are rejected.
        Sign->SetWorldLocationAndRotation(Position, Facing);
        Sign->RegisterComponent();
    }
    void Facade(const FVector& Center, const FVector& Size, int32 Style = 0, const FString& Name = TEXT("SEALED AUXILIARY BLOCK")) {
        const FLinearColor Frame(.10f, .12f, .13f), Trim(.40f, .42f, .39f), RoofColor(.12f, .15f, .16f);
        const FLinearColor Glass(.18f, .31f, .33f), GlassWarm(.48f, .43f, .29f), Plant(.27f, .32f, .32f);
        const FLinearColor Oxide(.34f, .23f, .14f), Copper(.34f, .30f, .20f), Stripe(.65f, .54f, .30f);
        const FLinearColor Skins[] = { FLinearColor(.29f, .31f, .28f), FLinearColor(.30f, .24f, .19f), FLinearColor(.23f, .30f, .31f) };
        const int32 Palette = FMath::Clamp(Style, 0, 2);
        const FName SkinKey(*FString::Printf(TEXT("Cladding%d"), Palette));
        const float Ground = Center.Z - Size.Z * .5f, Roof = Center.Z + Size.Z * .5f;
        auto Face = [&](const FVector& Origin, float Width, const FRotator& Rotation, bool Entrance) {
            const int32 Columns = FMath::Clamp(FMath::FloorToInt(Width / (Style == 2 ? 255.f : 325.f)), 2, 12);
            const int32 Rows = Size.Z >= 500.f ? 2 : 1;
            const float Bay = Width / Columns;
            auto Place = [&](FName Key, const FVector& Local, const FVector& Dimensions, const FLinearColor& Color,
                float Roughness = .8f, float Detail = 0.f, float Metalness = 0.f, float Emission = 0.f) {
                Add(Key, Origin + Rotation.RotateVector(Local), Dimensions, Color, Roughness, Detail,
                    Rotation, Metalness, Emission);
            };
            auto AtZ = [&](float Z) { return Z - Origin.Z; };
            // Shallow cladding relief touches the unchanged opaque collision hull.
            Place(SkinKey, FVector(0, 0, AtZ(Ground + Size.Z * .5f)), FVector(Width - 8, 8, Size.Z - 8), Skins[Palette], .86f, .38f);
            Place(TEXT("FoundationPlinth"), FVector(0, -7, AtZ(Ground + 48)), FVector(Width + 10, 16, 96), Frame, .84f, .25f);
            Place(TEXT("StoneCornice"), FVector(0, -11, AtZ(Roof - 20)), FVector(Width + 24, 26, 28), Trim, .82f, .25f);
            const float DoorX = -.5f * Width + .5f * Bay;
            for (int32 Column = 0; Column < Columns; ++Column) {
                const float X = (Column + .5f) * Bay - Width * .5f;
                for (int32 Row = 0; Row < Rows; ++Row) {
                    if (Entrance && Column == 0 && Row == 0) continue;
                    const float WindowZ = Ground + 166.f + Row * 210.f;
                    const float WindowWidth = FMath::Min(Bay - 55.f, Style == 2 ? 175.f : 150.f);
                    const float WindowHeight = Style == 1 ? 102.f : 116.f;
                    const bool Warm = (Column + Row * 3 + Style) % 5 == 2;
                    Place(Warm ? TEXT("WarmOpaqueGlass") : TEXT("SlateOpaqueGlass"), FVector(X, -8, AtZ(WindowZ)),
                        FVector(WindowWidth, 6, WindowHeight), Warm ? GlassWarm : Glass, .34f, 0.f, .12f, Warm ? .18f : .025f);
                    for (float Side : { -1.f, 1.f })
                        Place(TEXT("WindowFrames"), FVector(X + Side * (WindowWidth * .5f + 4), -12, AtZ(WindowZ)),
                            FVector(8, 12, WindowHeight + 18), Frame, .58f, 0.f, .32f);
                    for (float Side : { -1.f, 1.f })
                        Place(TEXT("WindowFrames"), FVector(X, -12, AtZ(WindowZ + Side * (WindowHeight * .5f + 4))),
                            FVector(WindowWidth + 16, 12, 8), Frame, .58f, 0.f, .32f);
                    Place(TEXT("WindowFrames"), FVector(X, -14, AtZ(WindowZ)), FVector(5, 11, WindowHeight), Frame, .58f, 0.f, .32f);
                    Place(TEXT("StoneCornice"), FVector(X, -19, AtZ(WindowZ - WindowHeight * .5f - 10)),
                        FVector(WindowWidth + 28, 35, 10), Trim, .82f, .25f);
                    // Uneven blinds are authored opaque strips, not a claim of visible interiors.
                    if (Warm) for (int32 Slat = 0; Slat < 3; ++Slat)
                        Place(TEXT("WindowBlinds"), FVector(X, -14, AtZ(WindowZ + WindowHeight * .5f - 15 - Slat * 13.f)),
                            FVector(WindowWidth - 9, 4, 4), Stripe, .82f);
                }
                const float JointX = X - Bay * .5f;
                Place(TEXT("StructuralRibs"), FVector(JointX, -10, AtZ(Ground + Size.Z * .5f)),
                    FVector(18, 21, Size.Z - 28), Trim, .84f, .27f);
                if (Style == 1) for (int32 Course = 0; Course < 8; ++Course)
                    Place(TEXT("MasonryCourses"), FVector(X, -5, AtZ(Ground + 95 + Course * 47.f)),
                        FVector(Bay - 25, 3, 3), Oxide, .91f, .12f);
            }
            if (Rows == 2) Place(TEXT("StoneCornice"), FVector(0, -10, AtZ(Ground + 282)),
                FVector(Width, 22, 14), Trim, .82f, .25f);
            const FVector DrainTop = Origin + Rotation.RotateVector(FVector(Width * .5f - 28, -26, AtZ(Roof - 28)));
            const FVector DrainBottom = Origin + Rotation.RotateVector(FVector(Width * .5f - 28, -26, AtZ(Ground + 22)));
            Pipe(TEXT("CopperDownpipes"), DrainBottom, DrainTop, 14, Copper, .55f);
            for (int32 Clip = 0; Clip < 3; ++Clip)
                Place(TEXT("WindowFrames"), FVector(Width * .5f - 28, -27, AtZ(Ground + 60 + Clip * 170.f)),
                    FVector(28, 12, 6), Frame, .58f, 0.f, .32f);
            if (Entrance) {
                const float DoorWidth = FMath::Min(Bay - 38, 230.f);
                Place(TEXT("SealedServiceDoors"), FVector(DoorX, -12, AtZ(Ground + 120)),
                    FVector(DoorWidth, 12, 210), Frame, .76f, .13f, .24f);
                for (int32 Slat = 0; Slat < 10; ++Slat)
                    Place(TEXT("RoofMetal"), FVector(DoorX, -20, AtZ(Ground + 30 + Slat * 19.f)),
                        FVector(DoorWidth - 14, 6, 5), Plant, .65f, .16f, .4f);
                Place(TEXT("StoneCornice"), FVector(DoorX, -40, AtZ(Ground + 240)),
                    FVector(DoorWidth + 36, 90, 14), Trim, .82f, .25f);
                Place(TEXT("SignBackplates"), FVector(0, -20, AtZ(Roof - 76)),
                    FVector(FMath::Min(Width - 45, 1050.f), 16, 62), Frame, .71f, 0.f, .25f);
                Label(Name, Origin + Rotation.RotateVector(FVector(0, -32, AtZ(Roof - 76))),
                    FRotator(0, Rotation.Yaw - 90.f, 0), FMath::Min(30.f, Width / 38.f));
                Place(TEXT("WarmFixtures"), FVector(DoorX, -36, AtZ(Ground + 216)),
                    FVector(64, 18, 12), FLinearColor(.72f, .63f, .40f), .4f, 0.f, .15f, .45f);
            }
        };
        Face(Center + FVector(0, -Size.Y * .5f - 4, 0), Size.X, FRotator::ZeroRotator, false);
        Face(Center + FVector(0, Size.Y * .5f + 4, 0), Size.X, FRotator(0, 180, 0), true);
        Face(Center + FVector(-Size.X * .5f - 4, 0, 0), Size.Y, FRotator(0, -90, 0), true);
        Face(Center + FVector(Size.X * .5f + 4, 0, 0), Size.Y, FRotator(0, 90, 0), false);
        Add(TEXT("RoofDecks"), FVector(Center.X, Center.Y, Roof + 3), FVector(Size.X - 16, Size.Y - 16, 6),
            RoofColor, .88f, .25f);
        for (float Side : { -1.f, 1.f }) {
            Add(TEXT("ParapetWalls"), FVector(Center.X, Center.Y + Side * (Size.Y * .5f - 15), Roof + 31),
                FVector(Size.X, 30, 62), Trim, .83f, .24f);
            Add(TEXT("ParapetWalls"), FVector(Center.X + Side * (Size.X * .5f - 15), Center.Y, Roof + 31),
                FVector(30, Size.Y - 60, 62), Trim, .83f, .24f);
            Add(TEXT("ParapetCaps"), FVector(Center.X, Center.Y + Side * (Size.Y * .5f - 15), Roof + 65),
                FVector(Size.X + 12, 42, 7), Frame, .59f, 0.f, FRotator::ZeroRotator, .42f);
            Add(TEXT("ParapetCaps"), FVector(Center.X + Side * (Size.X * .5f - 15), Center.Y, Roof + 65),
                FVector(42, Size.Y - 30, 7), Frame, .59f, 0.f, FRotator::ZeroRotator, .42f);
        }
        // Roof plant differs in silhouette from the earlier repeated three boxes.
        const FVector PlantCenter(Center.X + Size.X * .20f, Center.Y + Size.Y * .14f, Roof + 60);
        Add(TEXT("RoofMetal"), PlantCenter, FVector(250, 150, 104), Plant, .65f, .16f, FRotator::ZeroRotator, .4f);
        Add(TEXT("ParapetCaps"), PlantCenter + FVector(0, 0, 56), FVector(268, 168, 8), Frame, .59f, 0.f, FRotator::ZeroRotator, .42f);
        Add(TEXT("RoofFanHousings"), PlantCenter + FVector(0, 0, 72), FVector(96, 96, 24), Oxide, .58f, .18f,
            FRotator::ZeroRotator, .5f, 0.f, true);
        for (int32 Slat = 0; Slat < 8; ++Slat)
            Add(TEXT("WindowFrames"), PlantCenter + FVector(-103 + Slat * 29.f, -79, 0),
                FVector(9, 7, 76), Frame, .58f, 0.f, FRotator::ZeroRotator, .32f);
        Add(TEXT("RoofMetal"), FVector(Center.X - Size.X * .09f, PlantCenter.Y, Roof + 27),
            FVector(Size.X * .48f, 90, 44), Plant, .65f, .16f, FRotator::ZeroRotator, .4f);
        for (int32 Joint = 0; Joint < 4; ++Joint)
            Add(TEXT("ParapetCaps"), FVector(Center.X - Size.X * .29f + Joint * Size.X * .13f, PlantCenter.Y, Roof + 51),
                FVector(10, 102, 5), Frame, .59f, 0.f, FRotator::ZeroRotator, .42f);
        for (int32 Index = 0; Index < 2; ++Index) {
            const FVector Skylight(Center.X - Size.X * .20f + Index * Size.X * .35f, Center.Y - Size.Y * .22f, Roof + 30);
            Add(TEXT("ParapetCaps"), Skylight, FVector(200, 180, 44), Frame, .59f, 0.f, FRotator::ZeroRotator, .42f);
            Add(TEXT("RoofSkylights"), Skylight + FVector(0, 0, 30), FVector(182, 172, 10), Glass, .28f, 0.f,
                FRotator(12, 0, 0), .18f, .03f);
            for (int32 Mullion = 0; Mullion < 3; ++Mullion)
                Add(TEXT("ParapetCaps"), Skylight + FVector(-62 + Mullion * 62.f, 0, 36),
                    FVector(6, 184, 10), Frame, .59f, 0.f, FRotator(12, 0, 0), .42f);
        }
        const float PipeY = Center.Y - Size.Y * .34f;
        Pipe(TEXT("RoofCopperPipes"), FVector(Center.X - Size.X * .40f, PipeY, Roof + 26),
            FVector(Center.X + Size.X * .38f, PipeY, Roof + 26), 18, Copper);
        for (int32 Support = 0; Support < 5; ++Support)
            Add(TEXT("ParapetCaps"), FVector(Center.X - Size.X * .37f + Support * Size.X * .18f, PipeY, Roof + 13),
                FVector(14, 50, 20), Frame, .59f, 0.f, FRotator::ZeroRotator, .42f);
    }
};

bool InsideExtraction(const ABGOperation* Operation, const FVector& Location) {
    return FVector::DistSquared2D(Location, Operation->ExtractionCenter) <= FMath::Square(Operation->ExtractionRadius);
}
}

ABGNavBounds::ABGNavBounds() {
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RegisteredNavigationExtent"));
    Bounds->SetupAttachment(GetRootComponent());
    Bounds->SetBoxExtent(FVector(5300, 4300, 1200));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetCanEverAffectNavigation(false);
    Bounds->SetHiddenInGame(true);
}

ABGOperation::ABGOperation() {
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = ABGCamera::StaticClass();
    PlayerControllerClass = ABGCommander::StaticClass();
    HUDClass = ABGHUD::StaticClass();
    Random.Initialize(1337);
}

void ABGOperation::BeginPlay() {
    Super::BeginPlay();
    LoadDefinitions();
    BuildDistrict();
    Notify(TEXT("Acquire the research specialist. Escort them to northwest extraction. Approach and interact to secure the escort."));
}

void ABGOperation::LoadDefinitions() {
    Weapons.Reset();
    FString Text;
    if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Weapons.csv")))) {
        TArray<FString> Lines;
        Text.ParseIntoArrayLines(Lines, true);
        for (int32 Index = 1; Index < Lines.Num(); ++Index) {
            TArray<FString> Fields;
            Lines[Index].ParseIntoArray(Fields, TEXT(","), false);
            if (Fields.Num() != 11) continue;
            for (FString& Field : Fields) Field.TrimStartAndEndInline();
            FBGWeaponDefinition Definition;
            Definition.Id = FName(*Fields[0]); Definition.Label = Fields[1];
            float RangeMeters = 0, HearingMeters = 0;
            const bool Valid = LexTryParseString(RangeMeters, *Fields[2])
                && LexTryParseString(Definition.Damage, *Fields[3])
                && LexTryParseString(Definition.Interval, *Fields[4])
                && LexTryParseString(HearingMeters, *Fields[5])
                && LexTryParseString(Definition.Weight, *Fields[6])
                && LexTryParseString(Definition.Magazine, *Fields[7])
                && LexTryParseString(Definition.Reserve, *Fields[8])
                && LexTryParseString(Definition.SpreadDegrees, *Fields[9])
                && LexTryParseString(Definition.ReloadSeconds, *Fields[10]);
            if (!Valid || Definition.Id.IsNone() || FindWeapon(Definition.Id)
                || !FMath::IsFinite(RangeMeters) || RangeMeters <= 0 || RangeMeters > 100
                || !FMath::IsFinite(HearingMeters) || HearingMeters < 0 || HearingMeters > 200
                || !FMath::IsFinite(Definition.Damage) || Definition.Damage <= 0 || Definition.Damage > 500
                || !FMath::IsFinite(Definition.Interval) || Definition.Interval < .05f || Definition.Interval > 10
                || !FMath::IsFinite(Definition.Weight) || Definition.Weight <= 0 || Definition.Weight > 30
                || Definition.Magazine <= 0 || Definition.Magazine > 500 || Definition.Reserve < 0 || Definition.Reserve > 5000
                || !FMath::IsFinite(Definition.SpreadDegrees) || Definition.SpreadDegrees < 0 || Definition.SpreadDegrees > 45
                || !FMath::IsFinite(Definition.ReloadSeconds) || Definition.ReloadSeconds < .1f || Definition.ReloadSeconds > 20) continue;
            Definition.Range = RangeMeters * 100; Definition.HearingRadius = HearingMeters * 100;
            Weapons.Add(Definition);
        }
    }
    if (!FindWeapon(TEXT("compact_sidearm")) || !FindWeapon(TEXT("compact_automatic"))) {
        Weapons.Reset();
        FBGWeaponDefinition Sidearm;
        Sidearm.Id = TEXT("compact_sidearm"); Sidearm.Label = TEXT("Vesper compact sidearm");
        Sidearm.Range = 1200; Sidearm.Damage = 24; Sidearm.Interval = .55f;
        Sidearm.HearingRadius = 1600; Sidearm.Weight = 2; Sidearm.Magazine = 12; Sidearm.Reserve = 72;
        Sidearm.SpreadDegrees = 1.1f; Sidearm.ReloadSeconds = 1.5f;
        Weapons.Add(Sidearm);
        FBGWeaponDefinition Automatic = Sidearm;
        Automatic.Id = TEXT("compact_automatic"); Automatic.Label = TEXT("Rook compact automatic");
        Automatic.Range = 2000; Automatic.Damage = 10; Automatic.Interval = .14f;
        Automatic.HearingRadius = 2300; Automatic.Weight = 4; Automatic.Magazine = 30;
        Automatic.Reserve = 150; Automatic.SpreadDegrees = 2.5f; Automatic.ReloadSeconds = 2.2f;
        Weapons.Add(Automatic);
        UE_LOG(LogTemp, Warning, TEXT("BLACKGLASS weapon data unavailable or incomplete; using explicit foundation definitions."));
    }
}

const FBGWeaponDefinition* ABGOperation::FindWeapon(FName Id) const {
    for (const FBGWeaponDefinition& Definition : Weapons) if (Definition.Id == Id) return &Definition;
    return nullptr;
}

void ABGOperation::BuildDistrict() {
    UWorld* World = GetWorld();
    // Procedural runtime geometry uses dynamic lighting; it has no baked lightmaps.
    if (AWorldSettings* DistrictSettings = World->GetWorldSettings()) DistrictSettings->bForceNoPrecomputedLighting = true;
    const FLinearColor Concrete(.18f, .19f, .18f), Dark(.08f, .11f, .13f), Steel(.15f, .18f, .19f);
    const FLinearColor Road(.025f, .033f, .039f), Pavement(.22f, .22f, .19f), Mark(.66f, .61f, .43f);
    // Presentation-only continuation: the playable collision ground remains the original block.
    Block(World, FVector(0, 0, -120), FVector(18000, 15000, 20), FLinearColor(.07f, .09f, .09f), false);
    Block(World, FVector(0, 0, -50), FVector(9600, 7600, 100), Concrete);
    Block(World, FVector(-1700, 0, 6), FVector(1000, 7400, 12), Road, true, FRotator::ZeroRotator, true);
    Block(World, FVector(0, -1800, 6), FVector(9400, 1000, 12), Road, true, FRotator::ZeroRotator, true);
    Block(World, FVector(-2450, 2200, 6), FVector(4700, 1000, 12), Road, true, FRotator::ZeroRotator, true);
    Block(World, FVector(-2450, 0, 12), FVector(400, 7300, 24), Pavement);
    Block(World, FVector(-950, 0, 12), FVector(400, 7300, 24), Pavement);
    Block(World, FVector(0, -2500, 12), FVector(9300, 400, 24), Pavement);
    Block(World, FVector(-2600, 1500, 12), FVector(4100, 350, 24), Pavement);
    for (int32 Index = -7; Index <= 7; ++Index) {
        Block(World, FVector(-1700, Index * 470.f, 14), FVector(15, 170, 3), Mark, false);
        Block(World, FVector(Index * 590.f, -1800, 14), FVector(200, 15, 3), Mark, false);
    }
    // Low, bounded buildings preserve the foundation camera's tactical readability.
    Block(World, FVector(-3750, -650, 270), FVector(1500, 1700, 540), Dark);
    Block(World, FVector(-3700, 650, 200), FVector(1550, 550, 400), Concrete);
    Block(World, FVector(-3400, -3300, 210), FVector(2200, 600, 420), Steel);
    Block(World, FVector(1500, -3250, 260), FVector(3100, 650, 520), Dark);
    Block(World, FVector(200, 1250, 280), FVector(1200, 2200, 560), Steel);
    for (int32 Index = 0; Index < 5; ++Index)
        Block(World, FVector(-2980, -1180 + Index * 280.f, 280), FVector(12, 100, 210), Mark, false);
    // Facility perimeter: main gate south and narrow service access west.
    Block(World, FVector(1337.5f, -1000, 130), FVector(475, 50, 260), Concrete);
    Block(World, FVector(3062.5f, -1000, 130), FVector(2275, 50, 260), Concrete);
    Block(World, FVector(1100, 237.5f, 130), FVector(50, 2475, 260), Concrete);
    Block(World, FVector(1100, 2212.5f, 130), FVector(50, 775, 260), Concrete);
    Block(World, FVector(4200, 800, 130), FVector(50, 3600, 260), Concrete);
    Block(World, FVector(2650, 2600, 130), FVector(3100, 50, 260), Concrete);
    auto Gate = [this, World](int32 Id, FVector Position, FRotator Rotation) {
        ABGDoor* Door = World->SpawnActor<ABGDoor>(Position, Rotation);
        if (Door) { Door->EntityId = Id; Doors.Add(Door); }
    };
    Gate(700, FVector(1750, -1000, 125), FRotator::ZeroRotator);
    Gate(701, FVector(1100, 1650, 125), FRotator(0, 90, 0));
    // Roofless laboratory with an actual entrance; walls obstruct both navigation and fire.
    Block(World, FVector(3150, 1500, 10), FVector(1500, 1400, 20), Pavement);
    Block(World, FVector(2400, 1500, 120), FVector(40, 1400, 240), Concrete);
    Block(World, FVector(3900, 1500, 120), FVector(40, 1400, 240), Concrete);
    Block(World, FVector(3150, 2200, 120), FVector(1500, 40, 240), Concrete);
    Block(World, FVector(2580, 800, 120), FVector(360, 40, 240), Concrete);
    Block(World, FVector(3580, 800, 120), FVector(640, 40, 240), Concrete);
    Block(World, FVector(3450, 1650, 65), FVector(380, 100, 130), Steel);
    Block(World, FVector(2900, 1850, 55), FVector(200, 120, 110), Dark);
    // A reachable raised route: continuous sloped decks connect both ends to street level.
    const float RampLength = FMath::Sqrt(1250.f * 1250.f + 300.f * 300.f);
    const float RampPitch = FMath::RadiansToDegrees(FMath::Atan2(300.f, 1250.f));
    Block(World, FVector(-875, 3000, 150), FVector(RampLength, 420, 24), Steel, true, FRotator(RampPitch, 0, 0));
    Block(World, FVector(575, 3000, 300), FVector(1650, 420, 24), Steel);
    Block(World, FVector(2025, 3000, 150), FVector(RampLength, 420, 24), Steel, true, FRotator(-RampPitch, 0, 0));
    Block(World, FVector(ExtractionCenter.X, ExtractionCenter.Y, 15), FVector(1100, 1000, 4), FLinearColor(.15f, .32f, .25f), false);
    for (int32 Index = 0; Index < 4; ++Index)
        Block(World, FVector(-4400 + Index * 380.f, 3200, 35), FVector(60, 60, 70), Mark);
    // Original modular facade and flush street details: no collision, navigation, or save identity.
    FBlackglassDressing Dressing(World);
    Dressing.Facade(FVector(-3750, -650, 270), FVector(1500, 1700, 540), 1, TEXT("CIVIC LOGISTICS / SEALED"));
    Dressing.Facade(FVector(-3700, 650, 200), FVector(1550, 550, 400), 0, TEXT("ALLOCATION SERVICES / SEALED"));
    Dressing.Facade(FVector(-3400, -3300, 210), FVector(2200, 600, 420), 0, TEXT("DISTRICT SUPPLY / SEALED"));
    Dressing.Facade(FVector(1500, -3250, 260), FVector(3100, 650, 520), 2, TEXT("MUNICIPAL COMPLIANCE / SEALED"));
    Dressing.Facade(FVector(200, 1250, 280), FVector(1200, 2200, 560), 2, TEXT("PROCESS ANNEX / SEALED"));
    const FLinearColor Curb(.51f, .5f, .44f), Grate(.065f, .075f, .08f);
    const float CurbRows[] = { -3350.f, -2750.f, -700.f, 0.f, 700.f, 1400.f, 2950.f, 3500.f };
    for (float Y : CurbRows) for (float X : { -2205.f, -1195.f }) {
        Dressing.Add(TEXT("Curbs"), FVector(X, Y, 25), FVector(14, 530, 6), Curb, .89f);
        Dressing.Add(TEXT("DrainFrames"), FVector(X + (X < -1700 ? 40 : -40), Y + 120, 13),
            FVector(45, 90, 2), Grate, .58f);
        for (int32 Slat = 0; Slat < 4; ++Slat)
            Dressing.Add(TEXT("Curbs"), FVector(X + (X < -1700 ? 40 : -40), Y + 91 + Slat * 19.f, 15),
                FVector(37, 4, 2), Curb, .89f);
    }
    for (int32 Stripe = 0; Stripe < 10; ++Stripe) {
        Dressing.Add(TEXT("CrosswalkPaint"), FVector(-2080 + Stripe * 85.f, -1130, 15),
            FVector(48, 220, 2), Mark, .95f);
        Dressing.Add(TEXT("CrosswalkPaint"), FVector(-1070, -2180 + Stripe * 85.f, 15),
            FVector(220, 48, 2), Mark, .95f);
    }
    Dressing.Add(TEXT("ConcreteTrim"), FVector(1337.5f, -1000, 263), FVector(475, 62, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(3062.5f, -1000, 263), FVector(2275, 62, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(1100, 237.5f, 263), FVector(62, 2475, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(1100, 2212.5f, 263), FVector(62, 775, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(4200, 800, 263), FVector(62, 3600, 6), Curb, .88f);
    Dressing.Add(TEXT("ConcreteTrim"), FVector(2650, 2600, 263), FVector(3100, 62, 6), Curb, .88f);
    // Gate apertures and the roofless laboratory entrance remain visibly and physically clear.
    const FLinearColor Identity(.55f, .3f, .12f);
    Dressing.Add(TEXT("CorporateIdentity"), FVector(1460, -1030, 190), FVector(160, 4, 45), Identity, .7f);
    Dressing.Add(TEXT("CorporateIdentity"), FVector(2050, -1030, 190), FVector(160, 4, 45), Identity, .7f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(2400, 1500, 244), FVector(48, 1400, 8), Curb, .88f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(3900, 1500, 244), FVector(48, 1400, 8), Curb, .88f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(3150, 2200, 244), FVector(1500, 48, 8), Curb, .88f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(2580, 800, 244), FVector(360, 48, 8), Curb, .88f);
    Dressing.Add(TEXT("LaboratoryTrim"), FVector(3580, 800, 244), FVector(640, 48, 8), Curb, .88f);
    // Flush street marks and paving joints retain the actual foundation routes.
    const FLinearColor Joint(.13f, .15f, .15f), WornPaint(.49f, .48f, .39f), SignMetal(.10f, .12f, .13f);
    for (float X : { -2450.f, -950.f }) for (int32 Tile = 0; Tile < 37; ++Tile)
        Dressing.Add(TEXT("PavingJoints"), FVector(X, -3500 + Tile * 195.f, 24.7f), FVector(360, 2, .6f), Joint, .92f);
    for (int32 Tile = 0; Tile < 48; ++Tile)
        Dressing.Add(TEXT("PavingJoints"), FVector(-4500 + Tile * 190.f, -2500, 24.7f), FVector(2, 360, .6f), Joint, .92f);
    for (float X : { -2375.f, -2525.f, -875.f, -1025.f })
        Dressing.Add(TEXT("PavingJoints"), FVector(X, 0, 24.7f), FVector(2, 7250, .6f), Joint, .92f);
    for (float Y : { -2575.f, -2425.f })
        Dressing.Add(TEXT("PavingJoints"), FVector(0, Y, 24.7f), FVector(9250, 2, .6f), Joint, .92f);
    for (float Y : { -2300.f, 850.f, 2850.f }) {
        Dressing.Add(TEXT("LaneArrows"), FVector(-1440, Y, 15), FVector(24, 150, 2), WornPaint, .96f);
        Dressing.Add(TEXT("LaneArrows"), FVector(-1465, Y + 82, 15), FVector(20, 75, 2), WornPaint, .96f, 0.f, FRotator(0, -45, 0));
        Dressing.Add(TEXT("LaneArrows"), FVector(-1415, Y + 82, 15), FVector(20, 75, 2), WornPaint, .96f, 0.f, FRotator(0, 45, 0));
    }
    Dressing.Add(TEXT("LaneArrows"), FVector(-1930, -1400, 15), FVector(390, 22, 2), WornPaint, .96f);
    Dressing.Add(TEXT("LaneArrows"), FVector(-770, -2035, 15), FVector(22, 390, 2), WornPaint, .96f);
    for (int32 Stripe = 0; Stripe < 7; ++Stripe)
        Dressing.Add(TEXT("LoadingPaint"), FVector(-3630 + Stripe * 160.f, -2360, 25), FVector(86, 12, 2), Mark, .95f);
    // Decorative boundaries sit on top of existing solid walls, not in gate apertures.
    Dressing.Add(TEXT("SignBackplates"), FVector(2700, -1032, 184), FVector(910, 14, 62), SignMetal, .71f, 0.f, FRotator::ZeroRotator, .25f);
    Dressing.Label(TEXT("CALDER SYSTEMS // RESEARCH"), FVector(2700, -1042, 184), FRotator(0, -90, 0), 33);
    Dressing.Label(TEXT("ACCESS 01"), FVector(1340, -1042, 192), FRotator(0, -90, 0), 23);
    Dressing.Label(TEXT("SERVICE 02"), FVector(1068, 2110, 191), FRotator(0, 180, 0), 23);
    for (float X : { 1215.f, 1350.f, 1485.f, 2010.f, 2170.f, 2330.f, 2490.f, 2650.f, 2810.f, 2970.f, 3130.f, 3290.f, 3450.f, 3610.f, 3770.f, 3930.f, 4090.f }) {
        Dressing.Add(TEXT("PavingJoints"), FVector(X, -1026, 111), FVector(3, 3, 190), Joint, .92f);
        Dressing.Add(TEXT("PerimeterSkirting"), FVector(X, -1030, 43), FVector(112, 7, 32), SignMetal, .8f, .18f, FRotator::ZeroRotator, .18f);
    }
    for (float Y : { -770.f, -410.f, -50.f, 310.f, 670.f, 1030.f, 1390.f, 1990.f, 2350.f })
        Dressing.Add(TEXT("PavingJoints"), FVector(1074, Y, 120), FVector(3, 3, 220), Joint, .92f);
    for (float X : { 1450.f, 2040.f, 3920.f })
        Dressing.Add(TEXT("WarmFixtures"), FVector(X, -1038, 236), FVector(64, 18, 12),
            FLinearColor(.72f, .63f, .40f), .4f, 0.f, FRotator::ZeroRotator, .15f, .45f);
    // Light lab dressing shares the already colliding counter and wall footprints.
    Dressing.Add(TEXT("LaboratoryInlays"), FVector(3150, 1500, 20.8f), FVector(1400, 1290, 1.5f), FLinearColor(.17f, .23f, .23f), .78f, .22f);
    for (int32 Line = 0; Line < 7; ++Line)
        Dressing.Add(TEXT("LabFloorJoints"), FVector(2490 + Line * 220.f, 1500, 21.8f), FVector(2, 1290, .5f), Joint, .9f);
    for (int32 Line = 0; Line < 6; ++Line)
        Dressing.Add(TEXT("LabFloorJoints"), FVector(3150, 945 + Line * 220.f, 21.8f), FVector(1400, 2, .5f), Joint, .9f);
    Dressing.Add(TEXT("RoofMetal"), FVector(3450, 1650, 135), FVector(370, 95, 10), FLinearColor(.27f, .32f, .32f), .65f, .16f, FRotator::ZeroRotator, .4f);
    for (int32 Terminal = 0; Terminal < 3; ++Terminal) {
        const float X = 3325 + Terminal * 122.f;
        Dressing.Add(TEXT("LabTerminalShells"), FVector(X, 1653, 178), FVector(94, 66, 76), SignMetal, .63f, .1f, FRotator::ZeroRotator, .3f);
        Dressing.Add(TEXT("LabTerminalDisplays"), FVector(X, 1618, 186), FVector(75, 3, 48),
            FLinearColor(.19f, .43f, .40f), .35f, 0.f, FRotator::ZeroRotator, .1f, .3f);
        Dressing.Add(TEXT("LabTerminalShells"), FVector(X, 1610, 145), FVector(84, 24, 7), SignMetal, .63f, .1f, FRotator::ZeroRotator, .3f);
    }
    Dressing.Add(TEXT("RoofMetal"), FVector(2900, 1850, 119), FVector(188, 109, 18), FLinearColor(.27f, .32f, .32f), .65f, .16f, FRotator::ZeroRotator, .4f);
    Dressing.Add(TEXT("LabTerminalShells"), FVector(2900, 1850, 168), FVector(155, 90, 82), SignMetal, .63f, .1f, FRotator::ZeroRotator, .3f);
    Dressing.Add(TEXT("LabTerminalDisplays"), FVector(2900, 1803, 166), FVector(115, 4, 50),
        FLinearColor(.19f, .43f, .40f), .35f, 0.f, FRotator::ZeroRotator, .1f, .3f);
    Dressing.Label(TEXT("CALDER / PROTOTYPE CONTROL"), FVector(3150, 2170, 180), FRotator(0, -90, 0), 29);
    // Thin rail tubes remain outside the bridge's walkable centerline.
    const FLinearColor Rail(.32f, .37f, .37f);
    for (float Y : { 2808.f, 3192.f }) {
        Dressing.Pipe(TEXT("BridgeRails"), FVector(-1500, Y, 106), FVector(-250, Y, 406), 10, Rail, .65f);
        Dressing.Pipe(TEXT("BridgeRails"), FVector(-250, Y, 406), FVector(1400, Y, 406), 10, Rail, .65f);
        Dressing.Pipe(TEXT("BridgeRails"), FVector(1400, Y, 406), FVector(2650, Y, 106), 10, Rail, .65f);
        for (int32 Post = 0; Post < 12; ++Post) {
            const float X = -1420 + Post * 360.f;
            const float DeckZ = X < -250 ? (X + 1500) * .24f : X > 1400 ? (2650 - X) * .24f : 300.f;
            Dressing.Pipe(TEXT("BridgeRails"), FVector(X, Y, DeckZ + 20), FVector(X, Y, DeckZ + 105), 10, Rail, .65f);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("BLACKGLASS original visual dressing: %d instances in %d material batches; collision disabled."),
        Dressing.Instances, Dressing.Batches.Num());
    if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), FRotator(-55, -35, 0))) {
        if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent())) SunLight->SetForwardShadingPriority(1);
        Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sun->GetLightComponent()->SetCastShadows(true);
        Sun->GetLightComponent()->SetIntensity(3.5f);
        Sun->GetLightComponent()->SetLightColor(FLinearColor(.95f, .9f, .82f));
    }
    // Restrained opposite fill illuminates facades when the captured background is empty.
    if (ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1800), FRotator(-30, 145, 0))) {
        if (UDirectionalLightComponent* FillLight = Cast<UDirectionalLightComponent>(Fill->GetLightComponent())) FillLight->SetForwardShadingPriority(0);
        Fill->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Fill->GetLightComponent()->SetCastShadows(false);
        Fill->GetLightComponent()->SetIntensity(.65f);
        Fill->GetLightComponent()->SetLightColor(FLinearColor(.65f, .72f, .8f));
    }
    if (ASkyLight* Sky = World->SpawnActor<ASkyLight>()) {
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetIntensity(.9f);
        Sky->GetLightComponent()->SetLightColor(FLinearColor(.65f, .73f, .82f));
        Sky->GetLightComponent()->bLowerHemisphereIsBlack = true;
        Sky->GetLightComponent()->SetLowerHemisphereColor(FLinearColor(.08f, .1f, .12f));
        Sky->GetLightComponent()->RecaptureSky();
    }
    ABGNavBounds* Bounds = World->SpawnActor<ABGNavBounds>(FVector(0, 0, 350), FRotator::ZeroRotator);
    if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)) {
        if (Bounds) Navigation->OnNavigationBoundsUpdated(Bounds);
    }
    FTimerDelegate NavigationBuild;
    NavigationBuild.BindWeakLambda(this, [this]() {
        if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())) Navigation->Build();
    });
    World->GetTimerManager().SetTimerForNextTick(NavigationBuild);
    const TCHAR* OperativeNames[] = { TEXT("VOSS"), TEXT("KLINE"), TEXT("MORROW"), TEXT("KESTREL") };
    for (int32 Index = 0; Index < 4; ++Index)
        SpawnUnit(Index + 1, EBGRole::Operative, OperativeNames[Index], FVector(-3600 + (Index % 2) * 150.f, -2700 + (Index / 2) * 170.f, 125));
    for (int32 Index = 0; Index < 8; ++Index) {
        ABGUnit* Civilian = SpawnUnit(100 + Index, EBGRole::Civilian, FString::Printf(TEXT("District worker %02d"), Index + 1),
            FVector(-2470, -2200 + Index * 500.f, 125));
        if (Civilian) Civilian->PatrolPoints = { FVector(-2470, -2200, 100), FVector(-2470, 1300, 100),
            FVector(-900, 1300, 100), FVector(-900, -2200, 100) };
    }
    const FVector GuardPositions[] = { FVector(1650, -450, 125), FVector(3600, 0, 125), FVector(1800, 1800, 125), FVector(3100, 1100, 125) };
    for (int32 Index = 0; Index < 4; ++Index) {
        ABGUnit* Guard = SpawnUnit(200 + Index, EBGRole::Guard, FString::Printf(TEXT("Facility security %02d"), Index + 1), GuardPositions[Index]);
        if (Guard) Guard->PatrolPoints = { GuardPositions[Index], FVector(1600, 400, 100), FVector(2100, 1800, 100), FVector(3700, 400, 100) };
    }
    Specialist = SpawnUnit(500, EBGRole::Specialist, TEXT("Research specialist: Iona Vale"), FVector(3100, 1650, 125));
    Vehicle = World->SpawnActor<ABGVehicle>(FVector(-2950, -1800, 90), FRotator(0, 90, 0));
    if (Vehicle) { Vehicle->EntityId = 600; Vehicle->Occupants.Init(0, 6); }
}

ABGUnit* ABGOperation::SpawnUnit(int32 Id, EBGRole NewRole, const FString& Label, const FVector& Location) {
    if (ABGUnit* Existing = FindUnit(Id)) return Existing;
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ABGUnit* Unit = GetWorld()->SpawnActor<ABGUnit>(Location, FRotator::ZeroRotator, Parameters);
    if (Unit) {
        Unit->Initialize(Id, NewRole, Label);
        Unit->SetActorHiddenInGame(NewRole == EBGRole::Guard);
        Units.Add(Unit);
    }
    return Unit;
}

ABGUnit* ABGOperation::FindUnit(int32 Id) const {
    for (ABGUnit* Unit : Units) if (IsValid(Unit) && Unit->EntityId == Id) return Unit;
    return nullptr;
}

AActor* ABGOperation::FindEntity(int32 Id) const {
    if (ABGUnit* Unit = FindUnit(Id)) return Unit;
    if (IsValid(Vehicle) && Vehicle->EntityId == Id) return Vehicle;
    for (ABGDoor* Door : Doors) if (IsValid(Door) && Door->EntityId == Id) return Door;
    return nullptr;
}

bool ABGOperation::CanSee(const ABGUnit* Observer, const ABGUnit* Target) const {
    if (!IsValid(Observer) || !IsValid(Target) || !Observer->Alive()) return false;
    const float Range = Observer->UnitRole == EBGRole::Operative ? 2300.f : Observer->UnitRole == EBGRole::Guard ? 1800.f : 1000.f;
    if (FVector::DistSquared(Observer->EffectiveLocation(), Target->EffectiveLocation()) > FMath::Square(Range)) return false;
    FCollisionQueryParams Parameters(SCENE_QUERY_STAT(BlackglassSight), false);
    Parameters.AddIgnoredActor(Observer);
    if (Observer->IsSeated() && IsValid(Vehicle)) Parameters.AddIgnoredActor(Vehicle);
    FHitResult Hit;
    const FVector Start = Observer->EffectiveLocation() + FVector(0, 0, 45);
    const FVector End = Target->EffectiveLocation() + FVector(0, 0, 30);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Parameters)) return true;
    return Hit.GetActor() == Target || (Target->IsSeated() && Hit.GetActor() == Vehicle);
}

bool ABGOperation::IsDetected(const ABGUnit* Target) const {
    if (!IsValid(Target)) return false;
    if (Target->UnitRole != EBGRole::Guard) return true;
    for (const ABGUnit* Unit : Units)
        if (IsValid(Unit) && Unit->UnitRole == EBGRole::Operative && Unit->Alive() && CanSee(Unit, Target)) return true;
    return false;
}

void ABGOperation::ReportIncident(const FVector& Location, int32 SourceId, float Radius) {
    if (bRestoring || Outcome != EBGOutcome::Active || !FMath::IsFinite(Radius) || Radius <= 0) return;
    ABGUnit* Source = FindUnit(SourceId);
    bool GuardHeard = false;
    for (ABGUnit* Unit : Units) {
        if (!IsValid(Unit) || !Unit->Alive() || Unit->EntityId == SourceId
            || FVector::DistSquared(Unit->EffectiveLocation(), Location) > FMath::Square(Radius)) continue;
        const int32 ConfirmedSource = Source && CanSee(Unit, Source) ? SourceId : 0;
        Unit->HearIncident(Location, ConfirmedSource);
        GuardHeard |= Unit->UnitRole == EBGRole::Guard;
    }
    if (GuardHeard && (!Source || Source->UnitRole == EBGRole::Operative)) {
        const bool NewAlarm = AlarmSeconds <= 0;
        AlarmSeconds = FMath::Max(AlarmSeconds, 65.f);
        ReportedPosition = Location;
        if (NewAlarm) Notify(TEXT("Security alarm: local gunfire report. Response teams investigate the reported position."));
    }
}

void ABGOperation::AcquireSpecialist(ABGUnit* Controller) {
    if (bRestoring || Outcome != EBGOutcome::Active || !IsValid(Controller) || !Controller->Alive()
        || Controller->UnitRole != EBGRole::Operative || !IsValid(Specialist) || !Specialist->Alive()) return;
    if (Controller->IsSeated() || Specialist->IsSeated()
        || FVector::DistSquared2D(Controller->EffectiveLocation(), Specialist->EffectiveLocation()) > FMath::Square(350.f)
        || !CanSee(Controller, Specialist)) { Notify(TEXT("Specialist interaction requires a nearby operative with a clear line of sight.")); return; }
    bSpecialistAcquired = true; SpecialistLeaderId = Controller->EntityId;
    Specialist->StopOrders(false);
    Notify(TEXT("Specialist secured as an escort. Keep the operative alive and extract together."));
}

void ABGOperation::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);
    if (bRestoring || Outcome != EBGOutcome::Active) return;
    const float PreviousTime = SimulationSeconds;
    SimulationSeconds += DeltaSeconds;
    AlarmSeconds = FMath::Max(0.f, AlarmSeconds - DeltaSeconds);
    if (AlarmSeconds > 0 && Reinforcements < 4 && FMath::FloorToInt(SimulationSeconds / 12.f) > FMath::FloorToInt(PreviousTime / 12.f)) {
        ABGUnit* Response = SpawnUnit(204 + Reinforcements, EBGRole::Guard, TEXT("Corporate response"),
            FVector(3200 + Reinforcements * 130.f, 3350, 125));
        if (Response) {
            Response->LastKnown = ReportedPosition; Response->EvidenceSeconds = 20; Response->ThreatId = 0;
            FBGOrder Investigation; Investigation.Type = EBGOrderType::Move; Investigation.Location = ReportedPosition;
            Response->IssueOrder(Investigation, false);
            ++Reinforcements;
        }
    }
    for (ABGUnit* Unit : Units) if (IsValid(Unit) && Unit->UnitRole == EBGRole::Guard) Unit->SetActorHiddenInGame(!IsDetected(Unit));
    if (bSpecialistAcquired && IsValid(Specialist) && Specialist->Alive()) {
        ABGUnit* Leader = FindUnit(SpecialistLeaderId);
        if (!IsValid(Leader) || !Leader->Alive()) {
            if (SpecialistLeaderId != 0) { SpecialistLeaderId = 0; Specialist->StopOrders(); Notify(TEXT("Escort leader lost. Another operative must approach and interact to resume extraction.")); }
        } else if (Leader->IsSeated() && IsValid(Vehicle) && Vehicle->Health > 0) {
            if (!Specialist->IsSeated()) {
                if (FVector::DistSquared2D(Specialist->EffectiveLocation(), Vehicle->GetActorLocation()) <= FMath::Square(300.f)) Vehicle->Board(Specialist);
                else if (!Specialist->bHasOrder || Specialist->Order.Type != EBGOrderType::Board || Specialist->Order.TargetId != Vehicle->EntityId) {
                    FBGOrder Boarding; Boarding.Type = EBGOrderType::Board; Boarding.TargetId = Vehicle->EntityId;
                    Specialist->IssueOrder(Boarding, false);
                }
            }
        } else if (Specialist->IsSeated()) {
            if (IsValid(Vehicle)) Vehicle->Exit(Specialist);
        } else if (FVector::DistSquared2D(Specialist->EffectiveLocation(), Leader->EffectiveLocation()) > FMath::Square(250.f)
            && (!Specialist->bHasOrder || Specialist->Order.Type != EBGOrderType::Move
                || FVector::DistSquared2D(Specialist->Order.Location, Leader->EffectiveLocation() + FVector(-80, -80, 0)) > FMath::Square(180.f))) {
            FBGOrder Follow; Follow.Location = Leader->EffectiveLocation() + FVector(-80, -80, 0); Specialist->IssueOrder(Follow, false);
        }
    }
    EvaluateMission();
    if (Outcome == EBGOutcome::Active && FMath::FloorToInt(SimulationSeconds / 120.f) > FMath::FloorToInt(PreviousTime / 120.f)) SaveOperation(TEXT("autosave"));
}

void ABGOperation::EvaluateMission() {
    if (bRestoring || Outcome != EBGOutcome::Active) return;
    if (!IsValid(Specialist) || !Specialist->Alive()) { Fail(TEXT("Research specialist lost. Acquisition contract failed.")); return; }
    int32 Survivors = 0, AtExtraction = 0;
    for (const ABGUnit* Unit : Units) if (IsValid(Unit) && Unit->UnitRole == EBGRole::Operative && Unit->Alive()) {
        ++Survivors;
        if (InsideExtraction(this, Unit->EffectiveLocation())) ++AtExtraction;
    }
    if (Survivors == 0) { Fail(TEXT("All deployed operatives lost. The acquisition operation has failed.")); return; }
    if (bSpecialistAcquired && Survivors > 0 && AtExtraction == Survivors && InsideExtraction(this, Specialist->EffectiveLocation())) {
        Outcome = EBGOutcome::Success;
        if (!bRewardSettled) { Credits += 6000; bRewardSettled = true; }
        for (ABGUnit* Unit : Units) if (IsValid(Unit)) Unit->StopOrders(true);
        if (IsValid(Vehicle)) Vehicle->StopTravel();
        Notify(TEXT("Acquisition complete. Specialist and all surviving operatives extracted. Contract revenue: 6000 credits. Restart for another run."));
        SaveOperation(TEXT("autosave"));
    }
}

void ABGOperation::Fail(const FString& Reason) {
    if (bRestoring || Outcome != EBGOutcome::Active) return;
    Outcome = EBGOutcome::Failed;
    for (ABGUnit* Unit : Units) if (IsValid(Unit)) Unit->StopOrders(true);
    if (IsValid(Vehicle)) Vehicle->StopTravel();
    Notify(Reason);
    SaveOperation(TEXT("autosave"));
}

void ABGOperation::Notify(const FString& Message) {
    if (Notice == Message && NoticeExpires > SimulationSeconds) return;
    Notice = Message; NoticeExpires = SimulationSeconds + 7.f;
    UE_LOG(LogTemp, Display, TEXT("BLACKGLASS: %s"), *Message);
}

void ABGOperation::RestartOperation() {
    UGameplayStatics::SetGamePaused(this, false);
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
